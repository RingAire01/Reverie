# Platforms and Tauri parity

Reverie targets a Tauri-class desktop runtime. This document records the
multi-platform architecture and the feature gap against Tauri.

## Platform layer

The runtime is split into a platform-agnostic API and per-OS backends:

```
src/
  reverie.reo                     app-facing facade
  reverie/
    platform.reo                  selects the host backend (single import point)
    platform/
      windows.reo                 Win32 + WebView2 backend
      windows/
        abi/{structs,user32,kernel32}.reo
        messages.reo  window.reo  loop.reo  webview.reo
      macos/                      planned: Cocoa + WKWebView
      linux/                      planned: GTK + WebKitGTK
```

The common API keeps stable names (`reverie_create_window`,
`reverie_webview_open`, `reverie_run_loop`, ...). Each backend implements them
with its own windowing and web engine.

### Selecting a backend

RingEcho has no conditional compilation and imports are resolved at compile
time, so exactly one backend may be imported (importing all would duplicate
symbols and reference absent OS APIs). Options, in order of preference:

1. **Generated facade** — `reverie/platform.reo` is written per target by the
   build tooling (the `create-reverie` template or CI), importing one backend.
   Works today, no language change.
2. **`@cfg` attribute** — add `@cfg(target_os = "windows")` (or similar) to
   RingEcho so one file can declare all backends and the compiler keeps the
   matching one. Cleanest long term; tracked in the RingEcho roadmap.

Until (2) exists, Reverie uses (1).

## Backends

| Backend | Windowing | Web engine | Status |
| --- | --- | --- | --- |
| Windows | Win32 (`user32`) | WebView2 (COM, `WebView2Loader.dll`) | ✅ M0 + M1 |
| macOS | Cocoa / AppKit | WKWebView | planned |
| Linux | GTK | WebKitGTK | planned |

Cross-compiling and testing macOS/Linux requires those hosts (and their system
libraries); CI must build each on its own runner, as RingEcho already does.

## Tauri parity checklist

Runtime features, roughly in dependency order:

- [x] Win32 window + message loop
- [x] WebView2 host
- [x] Asset protocol (serve the bundled frontend; virtual host mapping on Windows)
- [x] IPC bridge (post message / poll on Windows; invoke/events/channels later)
- [ ] Config file + schema (`reverie.conf`)
- [ ] Permissions / capabilities model
- [ ] Multiple windows, DPI, transparency, fullscreen, decorations
- [ ] Menus, tray, native dialogs (open/save)
- [ ] Dev-server integration and HMR
- [x] Bundling: one-command build (`scripts/build-app.ps1`) with runtime DLLs
- [ ] Installers and resource embedding
- [ ] Auto-update and code signing
- [ ] Logging / telemetry
- [ ] macOS and Linux backends

## RingEcho prerequisites

Tracked in the RingEcho `ROADMAP.md`. Remaining for parity:

- T3 `int → fn` cast (dynamic dispatch)
- L1 capturing closures (stateful IPC callbacks)
- L2 global/static state (runtime singletons)
- L3 Windows threads/async (event loop, async I/O)
- L7 `@cfg` conditional compilation (backend selection above)
- L4 debug info, L5 `const`, L6 namespaces
