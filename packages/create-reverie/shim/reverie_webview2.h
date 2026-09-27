/* Reverie WebView2 shim - a plain C boundary over the WebView2 COM API.
 *
 * RingEcho cannot include system headers or link COM libraries directly, so the
 * Reo runtime talks to this shim instead. The shim is compiled to a static
 * library and linked into the runtime with `rev build --include ... --link ...`.
 */
#ifndef REVERIE_WEBVIEW2_H
#define REVERIE_WEBVIEW2_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start a WebView2 that fills `hwnd` and navigates to `url` (UTF-8).
 *
 * Asynchronous: returns 0 once the request is dispatched; poll
 * reverie_webview_state() for progress. Must be called on the thread that
 * pumps the window message loop. */
int reverie_webview_start(void *hwnd, const char *url);

/* 0 idle, 1 environment ready, 2 controller ready, 3 navigated, <0 failed. */
int reverie_webview_state(void);

/* Map a host name (for example "reverie.local") to a local folder, served over
 * https from the WebView2. Returns 0 on success. */
int reverie_webview_set_folder(const char *host, const char *dir);

/* Send a UTF-8 message to the page (window.chrome.webview "message" event). */
int reverie_webview_send(const char *message);

/* Register a push handler invoked with each message the page posts. Pass NULL
 * to clear. The handler runs on the UI thread. The memory is owned by the shim
 * and valid only for the duration of the call. */
void reverie_webview_on_message(void (*handler)(const char *message));

/* Navigation policy: only these hosts (plus data:/about:/blob:) may be loaded.
 * "reverie.local" and "localhost" are allowed by default; everything else is
 * blocked. Hosts are matched case-insensitively, ignoring a port. */
void reverie_webview_allow_host(const char *host);

/* Navigate the WebView2 to url. Subject to the navigation policy. */
int reverie_webview_navigate(const char *url);

/* URI of the most recent blocked navigation, or "" if none. */
const char *reverie_webview_last_blocked(void);

/* Next message posted by the page, or "" when none. The returned pointer is
 * owned by the shim and valid until the next call. */
const char *reverie_webview_poll(void);

/* Release the controller and environment. */
void reverie_webview_stop(void);

/* Last error as UTF-8; never NULL. */
const char *reverie_webview_error(void);

#ifdef __cplusplus
}
#endif

#endif /* REVERIE_WEBVIEW2_H */
