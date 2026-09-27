/* Reverie WebView2 shim - a plain C boundary over the WebView2 COM API.
 *
 * RingEcho cannot include system headers or link COM libraries directly, so the
 * Reo runtime talks to this shim instead. The shim is compiled to a static
 * library and linked into the runtime with `rev build --include ... --link ...`.
 *
 * Each top-level HWND owns one WebView2. Control functions take the window
 * handle; command/message handlers are app-global.
 */
#ifndef REVERIE_WEBVIEW2_H
#define REVERIE_WEBVIEW2_H

#ifdef __cplusplus
extern "C" {
#endif

/* Process-wide initialization: per-monitor DPI awareness. Call before windows. */
void reverie_win_init(void);

/* Append a timestamped line to reverie.log next to the executable. */
void reverie_log_write(const char *message);

/* System tray icon. Returns 1 on success. Events arrive at `hwnd` as WM_APP+1. */
int reverie_tray_add(void *hwnd, const char *tooltip);
void reverie_tray_remove(void);

/* Enforce a minimum/maximum content size on `hwnd` by subclassing it and
 * answering WM_GETMINMAXINFO. Zero means "no constraint". Returns 0 on success. */
int reverie_window_set_limits(void *hwnd, int min_width, int min_height,
                              int max_width, int max_height);

/* Navigation policy: only these hosts (plus data:/about:/blob:) may load.
 * "reverie.local" and "localhost" are allowed by default. */
void reverie_webview_allow_host(const char *host);

/* Native open/save dialogs. Return the chosen path (UTF-8) or "" if cancelled.
 * These block until the user responds. */
const char *reverie_dialog_open(const char *pattern);
const char *reverie_dialog_save(const char *pattern);

/* Start a WebView2 that fills `hwnd` and navigates to `url` (UTF-8).
 * Asynchronous: poll reverie_webview_state() for progress. */
int reverie_webview_start(void *hwnd, const char *url);

/* 0 idle, 1 environment ready, 2 controller ready, 3 navigated, <0 failed. */
int reverie_webview_state(void *hwnd);

/* Send a UTF-8 message to the window's page. */
int reverie_webview_send(void *hwnd, const char *message);

/* Next message the page posted, or "" when none. */
const char *reverie_webview_poll(void *hwnd);

/* Map a host name (for example "reverie.local") to a local folder. */
int reverie_webview_set_folder(void *hwnd, const char *host, const char *dir);

/* Navigate the window, subject to the navigation policy. */
int reverie_webview_navigate(void *hwnd, const char *url);

/* Register a push handler invoked with each message any page posts. Pass NULL
 * to clear. The handler memory is owned by the shim. */
void reverie_webview_on_message(void (*handler)(const char *message));

/* HWND of the window that produced the message currently being handled. */
void *reverie_webview_message_window(void);

/* Command registry (allowlist). A page message "name" or "name:arg" routes to
 * the handler registered for "name"; unregistered names are rejected. */
void reverie_webview_register_command(const char *name, void (*handler)(const char *arg));

/* Release the window's controller and environment. */
void reverie_webview_stop(void *hwnd);

/* Last error as UTF-8; never NULL. */
const char *reverie_webview_error(void);

/* URI of the most recent blocked navigation, or "". */
const char *reverie_webview_last_blocked(void);

#ifdef __cplusplus
}
#endif

#endif /* REVERIE_WEBVIEW2_H */
