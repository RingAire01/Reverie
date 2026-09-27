/* Reverie GTK/WebKitGTK shim implementation. See reverie_gtk.h. */

#include "reverie_gtk.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <jsc/jsc.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REVERIE_MAX_WINDOWS 8
#define REVERIE_MAX_COMMANDS 32

typedef struct {
    int used;
    int index;
    GtkWidget *window;
    WebKitWebView *webview;
    WebKitUserContentManager *ucm;
    int state; /* 0 idle, 2 created, 3 loaded, <0 failed */
    char message[4096];
    int has_message;
} ReverieWin;

typedef struct {
    char name[64];
    void (*handler)(const char *arg);
} ReverieCommand;

typedef struct {
    char host[256];
    char dir[1024];
} ReverieFolder;

static ReverieWin g_windows[REVERIE_MAX_WINDOWS];
static char g_error[256];
static char g_last_blocked[2048];
static char g_dialog_path[1024];
static void (*g_message_handler)(const char *) = NULL;
static void *g_message_window = NULL;

static char g_allowed_hosts[8][256];
static int g_allowed_host_count = 0;

static ReverieCommand g_commands[REVERIE_MAX_COMMANDS];
static int g_command_count = 0;

static ReverieFolder g_folders[8];
static int g_folder_count = 0;

static int g_initialized = 0;

/* ---- small helpers ---- */

static void set_error(const char *message) {
    snprintf(g_error, sizeof(g_error), "%s", message ? message : "unknown error");
}

/* The handle is the address of its ReverieWin, so lookups are O(1) and safe. */
static ReverieWin *find_window(void *hwnd) {
    ReverieWin *w = (ReverieWin *)hwnd;
    if (w < g_windows || w >= g_windows + REVERIE_MAX_WINDOWS) return NULL;
    if (((uintptr_t)w - (uintptr_t)g_windows) % sizeof(ReverieWin) != 0) return NULL;
    return w->used ? w : NULL;
}

static ReverieWin *alloc_window(void) {
    for (int i = 0; i < REVERIE_MAX_WINDOWS; i++) {
        if (!g_windows[i].used) {
            memset(&g_windows[i], 0, sizeof(g_windows[i]));
            g_windows[i].used = 1;
            g_windows[i].index = i;
            return &g_windows[i];
        }
    }
    return NULL;
}

static void set_file(char *out, size_t cap, const char *value) {
    if (!out || cap == 0) return;
    snprintf(out, cap, "%s", value ? value : "");
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
    set_file(g_allowed_hosts[g_allowed_host_count++], 256, "reverie.local");
    set_file(g_allowed_hosts[g_allowed_host_count++], 256, "localhost");
}

static int host_allowed(const char *uri) {
    if (!uri) return 0;
    const char *scheme = strstr(uri, "://");
    if (!scheme) return 1; /* about:, data:, file: handled elsewhere */
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

/* ---- asset folders + custom scheme ---- */

static const char *folder_for_host(const char *host) {
    for (int i = 0; i < g_folder_count; i++) {
        if (strcmp(g_folders[i].host, host) == 0) return g_folders[i].dir;
    }
    return NULL;
}

static void remember_folder(const char *host, const char *dir) {
    if (!host || !*host) return;
    for (int i = 0; i < g_folder_count; i++) {
        if (strcmp(g_folders[i].host, host) == 0) {
            set_file(g_folders[i].dir, sizeof(g_folders[i].dir), dir);
            return;
        }
    }
    if (g_folder_count >= 8) return;
    set_file(g_folders[g_folder_count].host, sizeof(g_folders[g_folder_count].host), host);
    set_file(g_folders[g_folder_count].dir, sizeof(g_folders[g_folder_count].dir), dir);
    g_folder_count++;
}

static void scheme_finish_error(WebKitURISchemeRequest *request, int code, const char *message) {
    GError *err = g_error_new_literal(g_quark_from_static_string("reverie-scheme"), code, message);
    webkit_uri_scheme_request_finish_error(request, err);
    g_error_free(err);
}

static void on_scheme_request(WebKitURISchemeRequest *request, gpointer user_data) {
    (void)user_data;
    const char *uri = webkit_uri_scheme_request_get_uri(request);
    const char *sep = uri ? strstr(uri, "://") : NULL;
    if (!sep) { scheme_finish_error(request, 400, "bad request"); return; }

    const char *host = sep + 3;
    const char *slash = strchr(host, '/');
    size_t host_len = slash ? (size_t)(slash - host) : strlen(host);
    if (host_len >= 256) { scheme_finish_error(request, 400, "bad host"); return; }
    char hostbuf[256];
    memcpy(hostbuf, host, host_len);
    hostbuf[host_len] = '\0';

    const char *dir = folder_for_host(hostbuf);
    if (!dir || !*dir) { scheme_finish_error(request, 404, "no asset folder"); return; }

    const char *path = slash ? slash + 1 : "";

    char rel[1024];
    set_file(rel, sizeof(rel), path);
    if (strstr(rel, "..")) { scheme_finish_error(request, 403, "forbidden"); return; }
    if (rel[0] == '\0' || rel[strlen(rel) - 1] == '/') {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "%sindex.html", rel);
        set_file(rel, sizeof(rel), tmp);
    }

    char full[2048];
    snprintf(full, sizeof(full), "%s/%s", dir, rel);

    GFile *file = g_file_new_for_path(full);
    GError *err = NULL;
    GFileInfo *info = g_file_query_info(file, "standard::type", G_FILE_QUERY_INFO_NONE, NULL, &err);
    if (!info) {
        g_object_unref(file);
        if (err) g_error_free(err);
        scheme_finish_error(request, 404, "not found");
        return;
    }
    GFileType type = g_file_info_get_file_type(info);
    g_object_unref(info);

    if (type == G_FILE_TYPE_DIRECTORY) {
        g_object_unref(file);
        char index[2048];
        snprintf(index, sizeof(index), "%s/index.html", full);
        file = g_file_new_for_path(index);
    }

    info = g_file_query_info(file, "standard::size", G_FILE_QUERY_INFO_NONE, NULL, &err);
    if (!info) {
        g_object_unref(file);
        if (err) g_error_free(err);
        scheme_finish_error(request, 404, "not found");
        return;
    }
    goffset size = g_file_info_get_size(info);
    g_object_unref(info);

    GFileInputStream *stream = g_file_read(file, NULL, &err);
    if (!stream) {
        g_object_unref(file);
        if (err) g_error_free(err);
        scheme_finish_error(request, 404, "not found");
        return;
    }

    gboolean uncertain = FALSE;
    char *content_type = g_content_type_guess(full, NULL, 0, &uncertain);
    webkit_uri_scheme_request_finish(request, G_INPUT_STREAM(stream),
                                     size, content_type ? content_type : "application/octet-stream");
    g_free(content_type);
    g_object_unref(stream);
    g_object_unref(file);
}

/* Rewrite "https://reverie.local/..." (or "http://") to the internal scheme. */
static void rewrite_asset_url(const char *url, char *out, size_t cap) {
    const char *sep = url ? strstr(url, "://") : NULL;
    if (sep) {
        const char *host = sep + 3;
        static const char needle[] = "reverie.local";
        size_t n = sizeof(needle) - 1;
        if (strncmp(host, needle, n) == 0 &&
            (host[n] == '/' || host[n] == ':' || host[n] == '\0')) {
            snprintf(out, cap, "reverie://reverie.local%s", host + n);
            return;
        }
    }
    snprintf(out, cap, "%s", url ? url : "");
}

/* ---- command registry (global) ---- */

static void dispatch_command(ReverieWin *w, const char *message) {
    char name[64];
    const char *colon = strchr(message, ':');
    size_t n = colon ? (size_t)(colon - message) : strlen(message);
    if (n >= sizeof(name)) n = sizeof(name) - 1;
    memcpy(name, message, n);
    name[n] = '\0';
    const char *arg = colon ? colon + 1 : "";
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) {
            g_message_window = w;
            g_commands[i].handler(arg);
            return;
        }
    }
    set_error("unknown command");
}

/* ---- WebKit callbacks ---- */

#if WEBKIT_CHECK_VERSION(2, 40, 0)
static void on_script_message(WebKitUserContentManager *ucm, JSCValue *value, gpointer user_data) {
    (void)ucm;
    ReverieWin *w = user_data;
    char *message = jsc_value_to_string(value);
    if (!message) return;
    set_file(w->message, sizeof(w->message), message);
    w->has_message = 1;
    if (g_command_count > 0) dispatch_command(w, w->message);
    else if (g_message_handler) { g_message_window = w; g_message_handler(w->message); }
    g_free(message);
}
#else
static void on_script_message(WebKitUserContentManager *ucm, WebKitJavascriptResult *result, gpointer user_data) {
    (void)ucm;
    ReverieWin *w = user_data;
    JSCValue *value = webkit_javascript_result_get_js_value(result);
    char *message = value ? jsc_value_to_string(value) : NULL;
    if (!message) return;
    set_file(w->message, sizeof(w->message), message);
    w->has_message = 1;
    if (g_command_count > 0) dispatch_command(w, w->message);
    else if (g_message_handler) { g_message_window = w; g_message_handler(w->message); }
    g_free(message);
}
#endif

static void on_load_changed(WebKitWebView *webview, WebKitLoadEvent event, gpointer user_data) {
    (void)webview;
    ReverieWin *w = user_data;
    if (event == WEBKIT_LOAD_STARTED) w->state = 1;
    else if (event == WEBKIT_LOAD_FINISHED) w->state = 3;
}

static gboolean on_load_failed(WebKitWebView *webview, WebKitLoadEvent event,
                               const gchar *failing_uri, GError *error, gpointer user_data) {
    (void)webview; (void)event; (void)failing_uri;
    ReverieWin *w = user_data;
    set_error(error && error->message ? error->message : "load failed");
    w->state = -1;
    return FALSE;
}

static void eval_js(WebKitWebView *webview, const char *script) {
#if WEBKIT_CHECK_VERSION(2, 40, 0)
    webkit_web_view_evaluate_javascript(webview, script, -1, NULL, NULL, NULL, NULL, NULL);
#else
    webkit_web_view_run_javascript(webview, script, NULL, NULL, NULL);
#endif
}

/* ---- init ---- */

void reverie_linux_init(void) {
    if (g_initialized) return;
    g_initialized = 1;

    int argc = 1;
    char arg0[] = "reverie";
    char *argv[] = { arg0, NULL };
    gtk_init_check(&argc, &argv);

    WebKitWebContext *ctx = webkit_web_context_get_default();
    webkit_web_context_register_uri_scheme(ctx, "reverie", on_scheme_request, NULL, NULL);
    WebKitSecurityManager *sm = webkit_web_context_get_security_manager(ctx);
    webkit_security_manager_register_uri_scheme_as_secure(sm, "reverie");
    webkit_security_manager_register_uri_scheme_as_cors_enabled(sm, "reverie");
#if WEBKIT_CHECK_VERSION(2, 32, 0)
    webkit_security_manager_register_uri_scheme_as_local(sm, "reverie");
#endif
}

void reverie_linux_log(const char *message) {
    FILE *f = fopen("reverie.log", "a");
    if (!f) return;
    fprintf(f, "[%lld] %s\n", (long long)time(NULL), message ? message : "");
    fclose(f);
}

/* ---- windows + loop ---- */

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ReverieWin *w = user_data;
    /* The webview is a child of the window, so GTK already destroyed it. */
    w->webview = NULL;
    w->ucm = NULL;
    w->used = 0;
}

void *reverie_linux_create_window(const char *class_name, const char *title, int width, int height) {
    (void)class_name;
    ReverieWin *w = alloc_window();
    if (!w) { set_error("too many windows"); return NULL; }

    w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (title) gtk_window_set_title(GTK_WINDOW(w->window), title);
    gtk_window_set_default_size(GTK_WINDOW(w->window), width > 0 ? width : 900,
                                height > 0 ? height : 600);
    g_signal_connect(w->window, "destroy", G_CALLBACK(on_window_destroy), w);
    gtk_widget_show_all(w->window);
    return w;
}

int reverie_linux_run_loop(void) {
    gtk_main();
    return 0;
}

static gboolean quit_timeout(gpointer user_data) {
    (void)user_data;
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

void reverie_linux_quit_after(void *hwnd, unsigned int ms) {
    (void)hwnd;
    g_timeout_add(ms, quit_timeout, NULL);
}

void *reverie_linux_default_proc(void *hwnd, unsigned int msg,
                                 unsigned long long wparam, unsigned long long lparam) {
    (void)hwnd; (void)msg; (void)wparam; (void)lparam;
    return NULL;
}

/* ---- navigation policy ---- */

void reverie_linux_allow_host(const char *host) {
    if (!host) return;
    ensure_default_hosts();
    for (int i = 0; i < g_allowed_host_count; i++)
        if (strcmp(g_allowed_hosts[i], host) == 0) return;
    if (g_allowed_host_count >= 8) return;
    set_file(g_allowed_hosts[g_allowed_host_count++], 256, host);
}

const char *reverie_linux_webview_last_blocked(void) { return g_last_blocked; }

/* ---- webview ---- */

int reverie_linux_webview_open(void *hwnd, const char *url) {
    ReverieWin *w = find_window(hwnd);
    if (!w) { set_error("unknown window"); return -1; }

    if (!w->webview) {
        w->ucm = webkit_user_content_manager_new();
        webkit_user_content_manager_register_script_message_handler(w->ucm, "reverie");
        g_signal_connect(w->ucm, "script-message-received::reverie", G_CALLBACK(on_script_message), w);

        w->webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_user_content_manager(w->ucm));
        g_signal_connect(w->webview, "load-changed", G_CALLBACK(on_load_changed), w);
        g_signal_connect(w->webview, "load-failed", G_CALLBACK(on_load_failed), w);
        gtk_container_add(GTK_CONTAINER(w->window), GTK_WIDGET(w->webview));
        gtk_widget_show_all(w->window);
        w->state = 2;
    }

    if (!url) return 0;
    char target[2048];
    rewrite_asset_url(url, target, sizeof(target));
    webkit_web_view_load_uri(w->webview, target);
    return 0;
}

int reverie_linux_webview_state(void *hwnd) {
    ReverieWin *w = find_window(hwnd);
    return w ? w->state : -1;
}

static void js_escape(const char *in, char *out, size_t cap) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)(in ? in : ""); *p && o + 7 < cap; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': out[o++] = '\\'; out[o++] = '"'; break;
            case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
            case '\n': out[o++] = '\\'; out[o++] = 'n'; break;
            case '\r': out[o++] = '\\'; out[o++] = 'r'; break;
            case '\t': out[o++] = '\\'; out[o++] = 't'; break;
            default:
                if (c < 0x20) o += (size_t)snprintf(out + o, cap - o, "\\u%04x", c);
                else out[o++] = (char)c;
        }
    }
    out[o] = '\0';
}

int reverie_linux_webview_send(void *hwnd, const char *message) {
    ReverieWin *w = find_window(hwnd);
    if (!w || !w->webview) { set_error("webview not ready"); return -1; }
    char escaped[8192];
    js_escape(message, escaped, sizeof(escaped));
    char script[8320];
    snprintf(script, sizeof(script),
             "if(window.__reverieReceive)window.__reverieReceive(\"%s\");", escaped);
    eval_js(w->webview, script);
    return 0;
}

const char *reverie_linux_webview_poll(void *hwnd) {
    ReverieWin *w = find_window(hwnd);
    if (!w || !w->has_message) return "";
    w->has_message = 0;
    return w->message;
}

int reverie_linux_webview_set_folder(void *hwnd, const char *host, const char *dir) {
    (void)hwnd;
    if (!dir) return -1;
    remember_folder(host ? host : "reverie.local", dir);
    return 0;
}

int reverie_linux_webview_navigate(void *hwnd, const char *url) {
    ReverieWin *w = find_window(hwnd);
    if (!w || !w->webview) { set_error("webview not ready"); return -1; }
    if (!host_allowed(url)) {
        set_file(g_last_blocked, sizeof(g_last_blocked), url);
        set_error("navigation blocked by policy");
        return -1;
    }
    char target[2048];
    rewrite_asset_url(url, target, sizeof(target));
    webkit_web_view_load_uri(w->webview, target);
    return 0;
}

void reverie_linux_webview_on_message(void (*handler)(const char *message)) {
    g_message_handler = handler;
}

void *reverie_linux_webview_message_window(void) {
    return g_message_window;
}

void reverie_linux_webview_register_command(const char *name, void (*handler)(const char *arg)) {
    if (!name || !handler) return;
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) { g_commands[i].handler = handler; return; }
    }
    if (g_command_count >= REVERIE_MAX_COMMANDS) return;
    set_file(g_commands[g_command_count].name, sizeof(g_commands[g_command_count].name), name);
    g_commands[g_command_count].handler = handler;
    g_command_count++;
}

void reverie_linux_webview_stop(void *hwnd) {
    ReverieWin *w = find_window(hwnd);
    if (!w) return;
    if (w->webview) { gtk_widget_destroy(GTK_WIDGET(w->webview)); w->webview = NULL; }
    w->state = 0;
    w->used = 0;
}

const char *reverie_linux_webview_error(void) {
    return g_error;
}

/* ---- tray (unsupported on plain GTK) ---- */

int reverie_linux_tray_add(void *hwnd, const char *tooltip) {
    (void)hwnd; (void)tooltip;
    set_error("system tray is not supported by the GTK backend");
    return 0;
}

void reverie_linux_tray_remove(void) {}

/* ---- native dialogs ---- */

static int run_dialog(int save, const char *pattern) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        save ? "Save File" : "Open File", NULL,
        save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        save ? "Save" : "Open", GTK_RESPONSE_ACCEPT, NULL);

    if (pattern && *pattern) {
        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "Files");
        gtk_file_filter_add_pattern(filter, pattern);
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    }

    g_dialog_path[0] = '\0';
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            set_file(g_dialog_path, sizeof(g_dialog_path), filename);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
    return 1;
}

const char *reverie_linux_dialog_open(const char *pattern) { run_dialog(0, pattern); return g_dialog_path; }
const char *reverie_linux_dialog_save(const char *pattern) { run_dialog(1, pattern); return g_dialog_path; }
