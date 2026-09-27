/* Reverie WebView2 shim - see reverie_webview2.h.
 *
 * WebView2 is COM. The shim owns the COM handlers and the asynchronous
 * environment/controller creation, and exposes a tiny blocking-free C API so a
 * RingEcho program can host a browser without touching COM directly. The loader
 * entry point is resolved dynamically from WebView2Loader.dll, so the shim has
 * no link-time dependency on the WebView2 SDK.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "WebView2.h"
#include "reverie_webview2.h"

typedef HRESULT (STDAPICALLTYPE *PFN_CreateEnv)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions *environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environmentCreatedHandler);

static ICoreWebView2Environment *g_env = NULL;
static ICoreWebView2Controller  *g_controller = NULL;
static ICoreWebView2           *g_webview = NULL;
static HWND g_hwnd = NULL;
static char g_url[2048];
static char g_error[256];
static int  g_state = 0;
static char g_message[4096];
static int  g_has_message = 0;
static EventRegistrationToken g_message_token;
static char g_pending_host[256];
static char g_pending_dir[1024];
static int  g_has_pending_folder = 0;

static int apply_folder(const char *host, const char *dir);

static void set_error(const char *message) {
    snprintf(g_error, sizeof(g_error), "%s", message ? message : "unknown error");
}

static void utf8_to_wide(const char *in, wchar_t *out, size_t cap) {
    if (!out || cap == 0) return;
    if (!in) { out[0] = 0; return; }
    if (MultiByteToWideChar(CP_UTF8, 0, in, -1, out, (int)cap) == 0) out[0] = 0;
}

/* ---- environment creation handler ---- */

static HRESULT STDMETHODCALLTYPE env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This,
                                        REFIID riid, void **ppv) {
    (void)riid;
    if (!ppv) return E_POINTER;
    *ppv = This;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This) {
    (void)This; return 1;
}
static ULONG STDMETHODCALLTYPE env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This) {
    (void)This; return 1;
}
static HRESULT STDMETHODCALLTYPE env_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This,
                                            HRESULT errorCode, ICoreWebView2Environment *environment);
static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl g_env_vtbl = {
    env_qi, env_addref, env_release, env_invoke
};
static struct { ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *lpVtbl; } g_env_handler;

/* ---- controller creation handler ---- */

static HRESULT STDMETHODCALLTYPE ctrl_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This,
                                         REFIID riid, void **ppv) {
    (void)riid;
    if (!ppv) return E_POINTER;
    *ppv = This;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE ctrl_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This) {
    (void)This; return 1;
}
static ULONG STDMETHODCALLTYPE ctrl_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This) {
    (void)This; return 1;
}
static HRESULT STDMETHODCALLTYPE ctrl_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This,
                                             HRESULT errorCode, ICoreWebView2Controller *controller);
static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_ctrl_vtbl = {
    ctrl_qi, ctrl_addref, ctrl_release, ctrl_invoke
};
static struct { ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *lpVtbl; } g_ctrl_handler;

/* ---- WebMessageReceived handler ---- */

static HRESULT STDMETHODCALLTYPE msg_qi(ICoreWebView2WebMessageReceivedEventHandler *This,
                                        REFIID riid, void **ppv) {
    (void)riid;
    if (!ppv) return E_POINTER;
    *ppv = This;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE msg_addref(ICoreWebView2WebMessageReceivedEventHandler *This) {
    (void)This; return 1;
}
static ULONG STDMETHODCALLTYPE msg_release(ICoreWebView2WebMessageReceivedEventHandler *This) {
    (void)This; return 1;
}
static HRESULT STDMETHODCALLTYPE msg_invoke(ICoreWebView2WebMessageReceivedEventHandler *This,
                                            ICoreWebView2 *sender,
                                            ICoreWebView2WebMessageReceivedEventArgs *args) {
    (void)This; (void)sender;
    LPWSTR text = NULL;
    if (args && SUCCEEDED(args->lpVtbl->TryGetWebMessageAsString(args, &text)) && text) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, g_message, (int)sizeof(g_message), NULL, NULL);
        g_has_message = 1;
        CoTaskMemFree(text);
    }
    return S_OK;
}
static ICoreWebView2WebMessageReceivedEventHandlerVtbl g_msg_vtbl = {
    msg_qi, msg_addref, msg_release, msg_invoke
};
static struct { ICoreWebView2WebMessageReceivedEventHandlerVtbl *lpVtbl; } g_msg_handler;

/* ---- callbacks ---- */

static HRESULT STDMETHODCALLTYPE env_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This,
                                            HRESULT errorCode, ICoreWebView2Environment *environment) {
    (void)This;
    if (FAILED(errorCode) || !environment) {
        set_error("WebView2 environment creation failed (is the WebView2 Runtime installed?)");
        g_state = -10;
        return S_OK;
    }
    g_env = environment;
    g_env->lpVtbl->AddRef(g_env);
    g_state = 1;

    g_ctrl_handler.lpVtbl = &g_ctrl_vtbl;
    HRESULT hr = g_env->lpVtbl->CreateCoreWebView2Controller(
        g_env, g_hwnd,
        (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)&g_ctrl_handler);
    if (FAILED(hr)) {
        set_error("CreateCoreWebView2Controller failed");
        g_state = -11;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ctrl_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This,
                                             HRESULT errorCode, ICoreWebView2Controller *controller) {
    (void)This;
    if (FAILED(errorCode) || !controller) {
        set_error("WebView2 controller creation failed");
        g_state = -20;
        return S_OK;
    }
    g_controller = controller;
    g_controller->lpVtbl->AddRef(g_controller);
    g_state = 2;

    ICoreWebView2 *webview = NULL;
    HRESULT hr = g_controller->lpVtbl->get_CoreWebView2(g_controller, &webview);
    if (FAILED(hr) || !webview) {
        set_error("get_CoreWebView2 failed");
        g_state = -21;
        return S_OK;
    }
    g_webview = webview;

    g_msg_handler.lpVtbl = &g_msg_vtbl;
    g_webview->lpVtbl->add_WebMessageReceived(
        g_webview,
        (ICoreWebView2WebMessageReceivedEventHandler *)&g_msg_handler,
        &g_message_token);

    RECT bounds;
    GetClientRect(g_hwnd, &bounds);
    g_controller->lpVtbl->put_Bounds(g_controller, bounds);

    if (g_has_pending_folder) {
        (void)apply_folder(g_pending_host, g_pending_dir);
        g_has_pending_folder = 0;
    }

    wchar_t wide[2048];
    utf8_to_wide(g_url, wide, 2048);
    hr = g_webview->lpVtbl->Navigate(g_webview, wide);
    if (FAILED(hr)) {
        set_error("Navigate failed");
        g_state = -22;
        return S_OK;
    }
    g_state = 3;
    return S_OK;
}

/* ---- public API ---- */

int reverie_webview_start(void *hwnd, const char *url) {
    g_hwnd = (HWND)hwnd;
    g_state = 0;
    g_error[0] = '\0';
    g_url[0] = '\0';
    if (url) snprintf(g_url, sizeof(g_url), "%s", url);

    (void)CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    HMODULE loader = LoadLibraryW(L"WebView2Loader.dll");
    if (!loader) {
        set_error("WebView2Loader.dll not found next to the executable");
        g_state = -1;
        return -1;
    }
    PFN_CreateEnv create = (PFN_CreateEnv)(void *)GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (!create) {
        set_error("CreateCoreWebView2EnvironmentWithOptions not exported");
        g_state = -2;
        return -1;
    }

    wchar_t user_data[MAX_PATH];
    wchar_t temp[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, temp);
    if (n == 0 || n >= MAX_PATH) swprintf(user_data, MAX_PATH, L"ReverieWebView2");
    else swprintf(user_data, MAX_PATH, L"%lsReverieWebView2", temp);

    g_env_handler.lpVtbl = &g_env_vtbl;
    HRESULT hr = create(NULL, user_data, NULL,
                        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&g_env_handler);
    if (FAILED(hr)) {
        set_error("CreateCoreWebView2EnvironmentWithOptions failed");
        g_state = -3;
        return -1;
    }
    return 0;
}

int reverie_webview_state(void) {
    return g_state;
}

static int apply_folder(const char *host, const char *dir) {
    if (!g_webview) { set_error("webview not ready"); return -1; }
    ICoreWebView2_3 *webview3 = NULL;
    HRESULT hr = g_webview->lpVtbl->QueryInterface(g_webview, &IID_ICoreWebView2_3, (void **)&webview3);
    if (FAILED(hr) || !webview3) {
        set_error("ICoreWebView2_3 unavailable");
        return -1;
    }
    wchar_t wide_host[256];
    wchar_t wide_dir[1024];
    utf8_to_wide(host, wide_host, 256);
    utf8_to_wide(dir, wide_dir, 1024);
    hr = webview3->lpVtbl->SetVirtualHostNameToFolderMapping(
        webview3, wide_host, wide_dir, COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
    webview3->lpVtbl->Release(webview3);
    if (FAILED(hr)) { set_error("SetVirtualHostNameToFolderMapping failed"); return -1; }
    return 0;
}

int reverie_webview_set_folder(const char *host, const char *dir) {
    /* The WebView2 is created asynchronously, so remember the mapping until the
     * controller (and its CoreWebView2) exists. */
    if (g_webview) return apply_folder(host, dir);
    if (host) snprintf(g_pending_host, sizeof(g_pending_host), "%s", host);
    if (dir) snprintf(g_pending_dir, sizeof(g_pending_dir), "%s", dir);
    g_has_pending_folder = 1;
    return 0;
}

int reverie_webview_send(const char *message) {
    if (!g_webview) { set_error("webview not ready"); return -1; }
    wchar_t wide[4096];
    utf8_to_wide(message, wide, 4096);
    HRESULT hr = g_webview->lpVtbl->PostWebMessageAsString(g_webview, wide);
    if (FAILED(hr)) { set_error("PostWebMessageAsString failed"); return -1; }
    return 0;
}

const char *reverie_webview_poll(void) {
    if (!g_has_message) return "";
    g_has_message = 0;
    return g_message;
}

void reverie_webview_stop(void) {
    if (g_webview) { g_webview->lpVtbl->Release(g_webview); g_webview = NULL; }
    if (g_controller) { g_controller->lpVtbl->Close(g_controller); g_controller->lpVtbl->Release(g_controller); g_controller = NULL; }
    if (g_env) { g_env->lpVtbl->Release(g_env); g_env = NULL; }
    g_state = 0;
}

const char *reverie_webview_error(void) {
    return g_error;
}
