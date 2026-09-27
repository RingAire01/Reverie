/* Reverie WebView2 shim - see reverie_webview2.h.
 *
 * Multi-window: each top-level HWND owns one WebView2. The control API is keyed
 * by HWND; command/message handlers stay app-global, and
 * reverie_webview_message_window() reports the window a dispatched message came
 * from so the app can reply to the right one.
 */
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "WebView2.h"
#include "reverie_webview2.h"

#define REVERIE_MAX_WINDOWS 8
#define REVERIE_MAX_COMMANDS 32

typedef struct {
    HWND hwnd;
    int index;
    int used;

    ICoreWebView2Environment *env;
    ICoreWebView2Controller *controller;
    ICoreWebView2 *webview;
    int state;

    char initial_url[2048];
    int has_pending_folder;
    char pending_host[256];
    char pending_dir[1024];

    char message[4096];
    int has_message;

    EventRegistrationToken msg_token;
    EventRegistrationToken nav_token;

    /* Per-window handler instances: vtable first, then a back-pointer. */
    struct { ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *lpVtbl; void *owner; } env_handler;
    struct { ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *lpVtbl; void *owner; } ctrl_handler;
    struct { ICoreWebView2WebMessageReceivedEventHandlerVtbl *lpVtbl; void *owner; } msg_handler;
    struct { ICoreWebView2NavigationStartingEventHandlerVtbl *lpVtbl; void *owner; } nav_handler;
} ReverieWindow;

typedef struct { ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *lpVtbl; void *owner; } EnvHandler;
typedef struct { ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *lpVtbl; void *owner; } CtrlHandler;
typedef struct { ICoreWebView2WebMessageReceivedEventHandlerVtbl *lpVtbl; void *owner; } MsgHandler;
typedef struct { ICoreWebView2NavigationStartingEventHandlerVtbl *lpVtbl; void *owner; } NavHandler;

static ReverieWindow g_windows[REVERIE_MAX_WINDOWS];
static char g_error[256];
static char g_last_blocked[2048];
static char g_dialog_path[1024];
static void (*g_message_handler)(const char *) = NULL;
static HWND g_message_window = NULL;

static char g_allowed_hosts[8][256];
static int g_allowed_host_count = 0;

typedef struct { char name[64]; void (*handler)(const char *arg); } ReverieCommand;
static ReverieCommand g_commands[REVERIE_MAX_COMMANDS];
static int g_command_count = 0;

static NOTIFYICONDATAW g_tray;
static int g_tray_active = 0;

static int apply_folder(ReverieWindow *w, const char *host, const char *dir);

static void utf8_to_wide(const char *in, wchar_t *out, size_t cap) {
    if (!out || cap == 0) return;
    if (!in) { out[0] = 0; return; }
    if (MultiByteToWideChar(CP_UTF8, 0, in, -1, out, (int)cap) == 0) out[0] = 0;
}

static void set_error(const char *message) {
    snprintf(g_error, sizeof(g_error), "%s", message ? message : "unknown error");
}

static ReverieWindow *find_window(HWND hwnd) {
    for (int i = 0; i < REVERIE_MAX_WINDOWS; i++)
        if (g_windows[i].used && g_windows[i].hwnd == hwnd) return &g_windows[i];
    return NULL;
}

static ReverieWindow *alloc_window(HWND hwnd) {
    ReverieWindow *existing = find_window(hwnd);
    if (existing) return existing;
    for (int i = 0; i < REVERIE_MAX_WINDOWS; i++) {
        if (!g_windows[i].used) {
            memset(&g_windows[i], 0, sizeof(g_windows[i]));
            g_windows[i].used = 1;
            g_windows[i].hwnd = hwnd;
            g_windows[i].index = i;
            return &g_windows[i];
        }
    }
    return NULL;
}

/* ---- navigation policy (global) ---- */

static int ci_equal_n(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        if (ca == '\0') return 1;
    }
    return 1;
}

static void ensure_default_hosts(void) {
    if (g_allowed_host_count > 0) return;
    snprintf(g_allowed_hosts[g_allowed_host_count++], 256, "%s", "reverie.local");
    snprintf(g_allowed_hosts[g_allowed_host_count++], 256, "%s", "localhost");
}

static int host_allowed(const char *uri) {
    if (!uri) return 0;
    const char *scheme = strstr(uri, "://");
    if (!scheme) return 1;
    const char *host = scheme + 3;
    size_t host_len = 0;
    while (host[host_len] && host[host_len] != '/' && host[host_len] != ':' &&
           host[host_len] != '?' && host[host_len] != '#') {
        host_len++;
    }
    ensure_default_hosts();
    for (int i = 0; i < g_allowed_host_count; i++) {
        if (strlen(g_allowed_hosts[i]) == host_len && ci_equal_n(host, g_allowed_hosts[i], host_len))
            return 1;
    }
    return 0;
}

/* ---- command registry (global) ---- */

static void dispatch_command(HWND hwnd, const char *message) {
    char name[64];
    const char *colon = strchr(message, ':');
    size_t n = colon ? (size_t)(colon - message) : strlen(message);
    if (n >= sizeof(name)) n = sizeof(name) - 1;
    memcpy(name, message, n);
    name[n] = '\0';
    const char *arg = colon ? colon + 1 : "";
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) {
            g_message_window = hwnd;
            g_commands[i].handler(arg);
            return;
        }
    }
    set_error("unknown command");
}

/* ---- COM handlers ---- */

static HRESULT STDMETHODCALLTYPE env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This, REFIID riid, void **ppv) {
    (void)riid; if (!ppv) return E_POINTER; *ppv = This; return S_OK;
}
static ULONG STDMETHODCALLTYPE env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This) { (void)This; return 1; }
static ULONG STDMETHODCALLTYPE env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This) { (void)This; return 1; }
static HRESULT STDMETHODCALLTYPE env_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This, HRESULT errorCode, ICoreWebView2Environment *environment);
static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl g_env_vtbl = { env_qi, env_addref, env_release, env_invoke };

static HRESULT STDMETHODCALLTYPE ctrl_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This, REFIID riid, void **ppv) {
    (void)riid; if (!ppv) return E_POINTER; *ppv = This; return S_OK;
}
static ULONG STDMETHODCALLTYPE ctrl_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This) { (void)This; return 1; }
static ULONG STDMETHODCALLTYPE ctrl_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This) { (void)This; return 1; }
static HRESULT STDMETHODCALLTYPE ctrl_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This, HRESULT errorCode, ICoreWebView2Controller *controller);
static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_ctrl_vtbl = { ctrl_qi, ctrl_addref, ctrl_release, ctrl_invoke };

static HRESULT STDMETHODCALLTYPE msg_qi(ICoreWebView2WebMessageReceivedEventHandler *This, REFIID riid, void **ppv) {
    (void)riid; if (!ppv) return E_POINTER; *ppv = This; return S_OK;
}
static ULONG STDMETHODCALLTYPE msg_addref(ICoreWebView2WebMessageReceivedEventHandler *This) { (void)This; return 1; }
static ULONG STDMETHODCALLTYPE msg_release(ICoreWebView2WebMessageReceivedEventHandler *This) { (void)This; return 1; }
static HRESULT STDMETHODCALLTYPE msg_invoke(ICoreWebView2WebMessageReceivedEventHandler *This, ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args);
static ICoreWebView2WebMessageReceivedEventHandlerVtbl g_msg_vtbl = { msg_qi, msg_addref, msg_release, msg_invoke };

static HRESULT STDMETHODCALLTYPE nav_qi(ICoreWebView2NavigationStartingEventHandler *This, REFIID riid, void **ppv) {
    (void)riid; if (!ppv) return E_POINTER; *ppv = This; return S_OK;
}
static ULONG STDMETHODCALLTYPE nav_addref(ICoreWebView2NavigationStartingEventHandler *This) { (void)This; return 1; }
static ULONG STDMETHODCALLTYPE nav_release(ICoreWebView2NavigationStartingEventHandler *This) { (void)This; return 1; }
static HRESULT STDMETHODCALLTYPE nav_invoke(ICoreWebView2NavigationStartingEventHandler *This, ICoreWebView2 *sender, ICoreWebView2NavigationStartingEventArgs *args);
static ICoreWebView2NavigationStartingEventHandlerVtbl g_nav_vtbl = { nav_qi, nav_addref, nav_release, nav_invoke };

static HRESULT STDMETHODCALLTYPE env_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This, HRESULT errorCode, ICoreWebView2Environment *environment) {
    ReverieWindow *w = (ReverieWindow *)((EnvHandler *)This)->owner;
    if (FAILED(errorCode) || !environment) {
        set_error("WebView2 environment creation failed (is the WebView2 Runtime installed?)");
        w->state = -10;
        return S_OK;
    }
    w->env = environment;
    w->env->lpVtbl->AddRef(w->env);
    w->state = 1;

    w->ctrl_handler.lpVtbl = &g_ctrl_vtbl;
    w->ctrl_handler.owner = w;
    HRESULT hr = w->env->lpVtbl->CreateCoreWebView2Controller(
        w->env, w->hwnd, (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)&w->ctrl_handler);
    if (FAILED(hr)) { set_error("CreateCoreWebView2Controller failed"); w->state = -11; }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ctrl_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This, HRESULT errorCode, ICoreWebView2Controller *controller) {
    ReverieWindow *w = (ReverieWindow *)((CtrlHandler *)This)->owner;
    if (FAILED(errorCode) || !controller) {
        set_error("WebView2 controller creation failed");
        w->state = -20;
        return S_OK;
    }
    w->controller = controller;
    w->controller->lpVtbl->AddRef(w->controller);
    w->state = 2;

    ICoreWebView2 *webview = NULL;
    HRESULT hr = w->controller->lpVtbl->get_CoreWebView2(w->controller, &webview);
    if (FAILED(hr) || !webview) { set_error("get_CoreWebView2 failed"); w->state = -21; return S_OK; }
    w->webview = webview;

    w->msg_handler.lpVtbl = &g_msg_vtbl;
    w->msg_handler.owner = w;
    w->webview->lpVtbl->add_WebMessageReceived(
        w->webview, (ICoreWebView2WebMessageReceivedEventHandler *)&w->msg_handler, &w->msg_token);

    w->nav_handler.lpVtbl = &g_nav_vtbl;
    w->nav_handler.owner = w;
    w->webview->lpVtbl->add_NavigationStarting(
        w->webview, (ICoreWebView2NavigationStartingEventHandler *)&w->nav_handler, &w->nav_token);

    RECT bounds;
    GetClientRect(w->hwnd, &bounds);
    w->controller->lpVtbl->put_Bounds(w->controller, bounds);

    if (w->has_pending_folder) {
        (void)apply_folder(w, w->pending_host, w->pending_dir);
        w->has_pending_folder = 0;
    }

    if (w->initial_url[0]) {
        wchar_t wide[2048];
        utf8_to_wide(w->initial_url, wide, 2048);
        if (FAILED(w->webview->lpVtbl->Navigate(w->webview, wide))) {
            set_error("Navigate failed");
            w->state = -22;
            return S_OK;
        }
    }
    w->state = 3;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE msg_invoke(ICoreWebView2WebMessageReceivedEventHandler *This, ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) {
    ReverieWindow *w = (ReverieWindow *)((MsgHandler *)This)->owner;
    (void)sender;
    LPWSTR text = NULL;
    if (args && SUCCEEDED(args->lpVtbl->TryGetWebMessageAsString(args, &text)) && text) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, w->message, (int)sizeof(w->message), NULL, NULL);
        w->has_message = 1;
        CoTaskMemFree(text);
        if (g_command_count > 0) dispatch_command(w->hwnd, w->message);
        else if (g_message_handler) { g_message_window = w->hwnd; g_message_handler(w->message); }
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE nav_invoke(ICoreWebView2NavigationStartingEventHandler *This, ICoreWebView2 *sender, ICoreWebView2NavigationStartingEventArgs *args) {
    (void)This; (void)sender;
    LPWSTR uri = NULL;
    if (args && SUCCEEDED(args->lpVtbl->get_Uri(args, &uri)) && uri) {
        char utf8[2048];
        WideCharToMultiByte(CP_UTF8, 0, uri, -1, utf8, (int)sizeof(utf8), NULL, NULL);
        if (!host_allowed(utf8)) {
            args->lpVtbl->put_Cancel(args, TRUE);
            snprintf(g_last_blocked, sizeof(g_last_blocked), "%s", utf8);
        }
        CoTaskMemFree(uri);
    }
    return S_OK;
}

static int apply_folder(ReverieWindow *w, const char *host, const char *dir) {
    if (!w || !w->webview) { set_error("webview not ready"); return -1; }
    ICoreWebView2_3 *webview3 = NULL;
    HRESULT hr = w->webview->lpVtbl->QueryInterface(w->webview, &IID_ICoreWebView2_3, (void **)&webview3);
    if (FAILED(hr) || !webview3) { set_error("ICoreWebView2_3 unavailable"); return -1; }
    wchar_t wide_host[256], wide_dir[1024];
    utf8_to_wide(host, wide_host, 256);
    utf8_to_wide(dir, wide_dir, 1024);
    hr = webview3->lpVtbl->SetVirtualHostNameToFolderMapping(
        webview3, wide_host, wide_dir, COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
    webview3->lpVtbl->Release(webview3);
    if (FAILED(hr)) { set_error("SetVirtualHostNameToFolderMapping failed"); return -1; }
    return 0;
}

/* ---- public API ---- */

void reverie_win_init(void) {
    typedef BOOL (WINAPI *PFN_SetDpiCtx)(void *);
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        PFN_SetDpiCtx setContext = (PFN_SetDpiCtx)(void *)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setContext && setContext((void *)(intptr_t)-4)) return;
    }
    SetProcessDPIAware();
}

void reverie_log_write(const char *message) {
    FILE *f = fopen("reverie.log", "a");
    if (!f) return;
    fprintf(f, "[%lld] %s\n", (long long)time(NULL), message ? message : "");
    fclose(f);
}

int reverie_tray_add(void *hwnd, const char *tooltip) {
    memset(&g_tray, 0, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = (HWND)hwnd;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = WM_APP + 1;
    g_tray.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
    utf8_to_wide(tooltip ? tooltip : "Reverie", g_tray.szTip, 128);
    if (!Shell_NotifyIconW(NIM_ADD, &g_tray)) { set_error("Shell_NotifyIcon failed"); return 0; }
    g_tray_active = 1;
    return 1;
}

void reverie_tray_remove(void) {
    if (!g_tray_active) return;
    Shell_NotifyIconW(NIM_DELETE, &g_tray);
    g_tray_active = 0;
}

static void build_dialog_filter(const char *pattern, wchar_t *filter) {
    wchar_t wide[128];
    utf8_to_wide(pattern && *pattern ? pattern : "*.*", wide, 128);
    wchar_t *p = filter;
    wcscpy(p, L"Files"); p += wcslen(p) + 1;
    wcscpy(p, wide); p += wcslen(p) + 1;
    wcscpy(p, L"All files"); p += wcslen(p) + 1;
    wcscpy(p, L"*.*"); p += wcslen(p) + 1;
    *p = L'\0';
}

static int run_dialog(int save, const char *pattern) {
    wchar_t file[MAX_PATH];
    file[0] = L'\0';
    wchar_t filter[512];
    build_dialog_filter(pattern, filter);

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    if (!ok) { g_dialog_path[0] = '\0'; return 0; }
    WideCharToMultiByte(CP_UTF8, 0, file, -1, g_dialog_path, (int)sizeof(g_dialog_path), NULL, NULL);
    return 1;
}

const char *reverie_dialog_open(const char *pattern) { run_dialog(0, pattern); return g_dialog_path; }
const char *reverie_dialog_save(const char *pattern) { run_dialog(1, pattern); return g_dialog_path; }

void reverie_webview_allow_host(const char *host) {
    if (!host) return;
    ensure_default_hosts();
    for (int i = 0; i < g_allowed_host_count; i++)
        if (strcmp(g_allowed_hosts[i], host) == 0) return;
    if (g_allowed_host_count >= 8) return;
    snprintf(g_allowed_hosts[g_allowed_host_count++], 256, "%s", host);
}

const char *reverie_webview_last_blocked(void) { return g_last_blocked; }

int reverie_webview_start(void *hwnd, const char *url) {
    ensure_default_hosts();
    ReverieWindow *w = alloc_window((HWND)hwnd);
    if (!w) { set_error("too many windows"); return -1; }
    if (url) snprintf(w->initial_url, sizeof(w->initial_url), "%s", url);
    w->state = 0;

    (void)CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    HMODULE loader = LoadLibraryW(L"WebView2Loader.dll");
    if (!loader) { set_error("WebView2Loader.dll not found next to the executable"); w->state = -1; return -1; }
    typedef HRESULT (STDAPICALLTYPE *PFN_CreateEnv)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
    PFN_CreateEnv create = (PFN_CreateEnv)(void *)GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (!create) { set_error("CreateCoreWebView2EnvironmentWithOptions not exported"); w->state = -2; return -1; }

    wchar_t user_data[MAX_PATH];
    wchar_t temp[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, temp);
    if (n == 0 || n >= MAX_PATH) swprintf(user_data, MAX_PATH, L"ReverieWebView2-%d", w->index);
    else swprintf(user_data, MAX_PATH, L"%lsReverieWebView2-%d", temp, w->index);

    w->env_handler.lpVtbl = &g_env_vtbl;
    w->env_handler.owner = w;
    HRESULT hr = create(NULL, user_data, NULL,
                        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&w->env_handler);
    if (FAILED(hr)) { set_error("CreateCoreWebView2EnvironmentWithOptions failed"); w->state = -3; return -1; }
    return 0;
}

int reverie_webview_state(void *hwnd) {
    ReverieWindow *w = find_window((HWND)hwnd);
    return w ? w->state : -1;
}

int reverie_webview_send(void *hwnd, const char *message) {
    ReverieWindow *w = find_window((HWND)hwnd);
    if (!w || !w->webview) { set_error("webview not ready"); return -1; }
    wchar_t wide[4096];
    utf8_to_wide(message, wide, 4096);
    if (FAILED(w->webview->lpVtbl->PostWebMessageAsString(w->webview, wide))) {
        set_error("PostWebMessageAsString failed");
        return -1;
    }
    return 0;
}

const char *reverie_webview_poll(void *hwnd) {
    ReverieWindow *w = find_window((HWND)hwnd);
    if (!w || !w->has_message) return "";
    w->has_message = 0;
    return w->message;
}

int reverie_webview_set_folder(void *hwnd, const char *host, const char *dir) {
    ReverieWindow *w = find_window((HWND)hwnd);
    if (!w) { set_error("unknown window"); return -1; }
    if (w->webview) return apply_folder(w, host, dir);
    if (host) snprintf(w->pending_host, sizeof(w->pending_host), "%s", host);
    if (dir) snprintf(w->pending_dir, sizeof(w->pending_dir), "%s", dir);
    w->has_pending_folder = 1;
    return 0;
}

int reverie_webview_navigate(void *hwnd, const char *url) {
    ReverieWindow *w = find_window((HWND)hwnd);
    if (!w || !w->webview) { set_error("webview not ready"); return -1; }
    wchar_t wide[2048];
    utf8_to_wide(url, wide, 2048);
    if (FAILED(w->webview->lpVtbl->Navigate(w->webview, wide))) { set_error("Navigate failed"); return -1; }
    return 0;
}

void reverie_webview_on_message(void (*handler)(const char *message)) {
    g_message_handler = handler;
}

void *reverie_webview_message_window(void) {
    return g_message_window;
}

void reverie_webview_register_command(const char *name, void (*handler)(const char *arg)) {
    if (!name || !handler) return;
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) { g_commands[i].handler = handler; return; }
    }
    if (g_command_count >= REVERIE_MAX_COMMANDS) return;
    snprintf(g_commands[g_command_count].name, sizeof(g_commands[g_command_count].name), "%s", name);
    g_commands[g_command_count].handler = handler;
    g_command_count++;
}

void reverie_webview_stop(void *hwnd) {
    ReverieWindow *w = find_window((HWND)hwnd);
    if (!w) return;
    if (w->webview) { w->webview->lpVtbl->Release(w->webview); w->webview = NULL; }
    if (w->controller) { w->controller->lpVtbl->Close(w->controller); w->controller->lpVtbl->Release(w->controller); w->controller = NULL; }
    if (w->env) { w->env->lpVtbl->Release(w->env); w->env = NULL; }
    w->state = 0;
    w->used = 0;
}

const char *reverie_webview_error(void) {
    return g_error;
}
