// Typed bridge to the Reverie runtime.
//
// The runtime registers commands (see reverie_register_command). The page calls
// them with invoke("name") or invoke("name", "arg"), and listens for replies the
// native side sends with reverie_send.

interface ReverieWebView {
  postMessage(message: string): void;
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
  }
}

const webview = (): ReverieWebView | undefined => window.chrome?.webview;

/** True when running inside the Reverie runtime. */
export const isReverie = (): boolean => webview() !== undefined;

/** Call a native command. `invoke("greet", "world")` sends "greet:world". */
export function invoke(command: string, arg?: string): void {
  const message = arg === undefined ? command : `${command}:${arg}`;
  webview()?.postMessage(message);
}

/** Subscribe to messages from the native side. Returns an unsubscribe function. */
export function onMessage(handler: (message: string) => void): () => void {
  const instance = webview();
  if (!instance) return () => {};
  const listener = (event: MessageEvent<string>) => handler(String(event.data));
  instance.addEventListener("message", listener);
  return () => instance.removeEventListener("message", listener);
}
