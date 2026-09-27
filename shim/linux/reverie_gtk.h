/* Reverie GTK/WebKitGTK shim - the Linux counterpart of reverie_webview2.h.
 *
 * RingEcho cannot include system headers or drive GObject directly, so the Reo
 * Linux backend (src/reverie/platform/linux.reo) binds these symbols. Unlike the
 * Windows shim, GTK owns the window and the main loop, so window creation and
 * iteration live here too.
 *
 * Assets are served through a custom URI scheme: the shim rewrites
 * "https://reverie.local/..." to "reverie://reverie.local/..." and maps the host
 * to a local folder (see reverie_linux_webview_set_folder).
 *
 * Build with shim/linux/build.sh (requires gtk+-3.0 and webkit2gtk-4.1).
 */
#ifndef REVERIE_GTK_H
#define REVERIE_GTK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Process-wide init: gtk_init, URI scheme + security manager setup. Call first. */
void reverie_linux_init(void);

/* Append a timestamped line to reverie.log next to the executable. */
void reverie_linux_log(const char *message);

/* Create a top-level window. `class_name` is ignored (GTK has no window class).
 * Returns an opaque handle used by every other function, or NULL on failure. */
void *reverie_linux_create_window(const char *class_name, const char *title, int width, int height);

/* Run the GTK main loop until the last window closes. Returns an exit code. */
int reverie_linux_run_loop(void);

/* Quit the main loop after `ms` milliseconds. */
void reverie_linux_quit_after(void *hwnd, unsigned int ms);

/* Unused compatibility hook (the app's WNDPROC analogue); returns NULL. */
void *reverie_linux_default_proc(void *hwnd, unsigned int msg,
                                 unsigned long long wparam, unsigned long long lparam);

/* Navigation policy: only these hosts (plus data:/about:/blob:) may load.
 * "reverie.local" and "localhost" are allowed by default. */
void reverie_linux_allow_host(const char *host);

/* Attach a WebKitWebView to `hwnd` and load `url` (UTF-8); async. */
int reverie_linux_webview_open(void *hwnd, const char *url);

/* 0 idle, 2 webview created, 3 load finished, <0 failed. */
int reverie_linux_webview_state(void *hwnd);

/* Send a UTF-8 message to the window's page (calls window.__reverieReceive). */
int reverie_linux_webview_send(void *hwnd, const char *message);

/* Next message the page posted, or "" when none. */
const char *reverie_linux_webview_poll(void *hwnd);

/* Map a host name (for example "reverie.local") to a local folder. */
int reverie_linux_webview_set_folder(void *hwnd, const char *host, const char *dir);

/* Navigate the window, subject to the navigation policy. */
int reverie_linux_webview_navigate(void *hwnd, const char *url);

/* Register a push handler invoked with each message any page posts. NULL clears. */
void reverie_linux_webview_on_message(void (*handler)(const char *message));

/* Handle of the window that produced the message currently being handled. */
void *reverie_linux_webview_message_window(void);

/* Command registry (allowlist): "name" or "name:arg" routes to the handler. */
void reverie_linux_webview_register_command(const char *name, void (*handler)(const char *arg));

/* Destroy the window's webview. */
void reverie_linux_webview_stop(void *hwnd);

/* Last error as UTF-8; never NULL. */
const char *reverie_linux_webview_error(void);

/* URI of the most recent blocked navigation, or "". */
const char *reverie_linux_webview_last_blocked(void);

/* System tray. GTK has no portable tray; these are best-effort no-ops that
 * report unsupported. */
int reverie_linux_tray_add(void *hwnd, const char *tooltip);
void reverie_linux_tray_remove(void);

/* Native open/save dialogs. Return the chosen path (UTF-8) or "" if cancelled. */
const char *reverie_linux_dialog_open(const char *pattern);
const char *reverie_linux_dialog_save(const char *pattern);

/* Runtime window control, mirroring the Windows API. */
void reverie_linux_window_set_title(void *hwnd, const char *title);
void reverie_linux_window_set_position(void *hwnd, int x, int y);
void reverie_linux_window_set_size(void *hwnd, int width, int height);
void reverie_linux_window_center(void *hwnd, int width, int height);
void reverie_linux_window_set_opacity(void *hwnd, int alpha);
void reverie_linux_window_set_always_on_top(void *hwnd, int on);
void reverie_linux_window_minimize(void *hwnd);
void reverie_linux_window_maximize(void *hwnd);
void reverie_linux_window_restore(void *hwnd);
void reverie_linux_window_show(void *hwnd);
void reverie_linux_window_hide(void *hwnd);
void reverie_linux_window_close(void *hwnd);
void reverie_linux_window_set_limits(void *hwnd, int min_width, int min_height,
                                     int max_width, int max_height);

#ifdef __cplusplus
}
#endif

#endif /* REVERIE_GTK_H */
