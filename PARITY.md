# Tauri parity plan

Goal: make Reverie functionally comparable to Tauri (windowing, IPC, security,
config, OS integration, bundling, cross-platform).

Legend: ✅ done · 🚧 partial · ⬜ not started

## Feature matrix

| Area | Tauri mechanism | Reverie status | Plan / blocker |
| --- | --- | --- | --- |
| Web engine | System WebView (WebView2/WKWebView/WebKitGTK) | ✅ Windows | macOS/Linux backends |
| Window | Window API, multi-window, DPI, decorations | 🚧 single window | multi-window + DPI + decorations |
| IPC request/response | `invoke` commands, serde | 🚧 `postMessage` + `reverie_send` | command dispatcher layer |
| IPC events (push) | `emit` / listeners | ✅ single handler via `reverie_on_message` | per-window/multi handler needs closures |
| Asset protocol | custom protocol | ✅ virtual host mapping | scope to the app folder |
| Security: CSP | `security.csp` | ⬜ | inject CSP header/meta per window |
| Security: permissions | capabilities + per-command allowlist | ⬜ | allowlist checked in the dispatcher |
| Security: isolation | per-window scope, no remote by default | ✅ `reverie_allow_host` + navigation policy | per-window allowlists later |
| Config | `tauri.conf.json` + schema | 🚧 `ringecho.toml` | `reverie.conf` + JSON schema |
| Menus / tray | Menu/Tray APIs | ⬜ | Win32 menus; needs a command bridge |
| Native dialogs | dialog plugin (open/save/message) | ⬜ | COM `IFileDialog` in the shim |
| Updater | updater plugin (signed) | ⬜ | download + verify + swap, later |
| Logging | log plugin | ⬜ | shim-level log file |
| Bundle / installers | Tauri bundler (NSIS/dmg/deb/AppImage) | 🚧 one-command build | NSIS installer; embedding |
| Dev server + HMR | `devUrl` | ⬜ | point the window at the Vite URL |
| Plugins | plugin system | ⬜ | command namespaces |
| Cross-platform | Win/mac/Linux/iOS/Android | 🚧 Windows | mac/linux backends; `@cfg` |
| Small binary | system webview | ✅ ~125 KB exe | keep |

## Security model (the part Tauri invests most in)

Tauri's security rests on four things; Reverie should mirror them:

1. **Capabilities/permissions** — a declarative set of commands a window may
   call. In Reverie the dispatcher checks the command name against an allowlist
   loaded from `reverie.conf` before invoking any handler.
2. **CSP** — a default Content-Security-Policy for the app pages, overridable in
   config, enforced by the webview.
3. **Isolation / no remote by default** — only the app's own origin
   (`reverie.local`) is trusted; arbitrary navigation is denied unless listed.
4. **Scoped assets** — `reverie_asset_folder` maps exactly one folder and
   nothing else on disk.

## Workstreams (priority order)

1. **IPC成型** — command dispatcher (request/response) + push events via a C
   callback registry. Unblocks menus, dialogs, and config APIs.
2. **Security** — allowlist + CSP + deny-remote-by-default.
3. **Config** — `reverie.conf` (JSON) + schema; feeds 1 and 2.
4. **Window features** — multi-window, DPI, decorations, fullscreen.
5. **OS integration** — menus/tray, file dialogs.
6. **Packaging** — NSIS installer, resource embedding, signing.
7. **Cross-platform** — macOS/Linux backends + per-OS CI.
8. **RingEcho language** — capturing closures (clean push callbacks), globals,
   `@cfg`, Windows threads/async.

## Where the C shim fits

Reverie keeps the web-engine and OS details in the C shim
(`shim/reverie_webview2.c`) and exposes a small C API to `.reo`. This is the
same layering Tauri uses (`wry`/`tao` + `windows-rs` under Rust). As RingEcho
grows closures, globals and threads, more orchestration can move from the shim
into `.reo`.
