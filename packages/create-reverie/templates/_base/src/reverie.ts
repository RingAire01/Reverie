// Typed bridge to the Reverie runtime.
//
// The runtime registers commands (see reverie_register_command). The page calls
// them with invoke("name") or invoke("name", "arg"), and listens for replies the
// native side sends with reverie_send.
//
// Two native transports are supported: the WebView2 bridge on Windows
// (window.chrome.webview) and the WebKitGTK bridge on Linux
// (window.webkit.messageHandlers.reverie, which delivers into
// window.__reverieReceive).

interface ReverieBridge {
  postMessage(message: string): void;
}

interface ReverieWebView extends ReverieBridge {
  addEventListener(
    type: "message",
    listener: (event: MessageEvent<string>) => void,
  ): void;
  removeEventListener(
    type: "message",
    listener: (event: MessageEvent<string>) => void,
  ): void;
}

declare global {
  interface Window {
    chrome?: {
      webview?: ReverieWebView;
    };
    webkit?: {
      messageHandlers?: { reverie?: ReverieBridge };
    };
    __reverieReceive?: (message: string) => void;
  }
}

const webview2 = (): ReverieWebView | undefined => window.chrome?.webview;
const webkit = (): ReverieBridge | undefined => window.webkit?.messageHandlers?.reverie;

/** True when running inside the Reverie runtime. */
export const isReverie = (): boolean => webview2() !== undefined || webkit() !== undefined;

/** Colour scheme injected by the host: "light", "dark", or "system". */
export const reverieTheme = (): string =>
  (typeof document !== "undefined" && document.documentElement.dataset.reverieTheme) || "system";

/** Call a native command. `invoke("greet", "world")` sends "greet:world". */
export function invoke(command: string, arg?: string): void {
  const message = arg === undefined ? command : `${command}:${arg}`;
  const instance = webview2();
  if (instance) {
    instance.postMessage(message);
    return;
  }
  webkit()?.postMessage(message);
}

const listeners = new Set<(message: string) => void>();

function ensureWebKitReceiver(): void {
  if (window.__reverieReceive) return;
  window.__reverieReceive = (message: string) => {
    for (const listener of listeners) listener(String(message));
  };
}

/** Subscribe to messages from the native side. Returns an unsubscribe function. */
export function onMessage(handler: (message: string) => void): () => void {
  const instance = webview2();
  if (instance) {
    const listener = (event: MessageEvent<string>) => handler(String(event.data));
    instance.addEventListener("message", listener);
    return () => instance.removeEventListener("message", listener);
  }
  if (webkit()) {
    ensureWebKitReceiver();
    listeners.add(handler);
    return () => {
      listeners.delete(handler);
    };
  }
  return () => {};
}
