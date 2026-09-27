# Platforms and Tauri parity

Reverie targets a Tauri-class desktop runtime. This document records the
multi-platform architecture and the feature gap against Tauri.

## Platform layer

The runtime is split into a platform-agnostic API and per-OS backends:

```
src/
  reverie.reo                     app-facing facade
  reverie/
    platform.reo                  imports all backends
    platform/
      windows.reo                 Win32 + WebView2 backend
      windows/
        abi/{structs,user32,kernel32}.reo
        messages.reo  window.reo  loop.reo  webview.reo
      macos.reo                   Cocoa + WKWebView backend (skeleton)
      linux.reo                   GTK + WebKitGTK backend (skeleton)
```

The common API keeps stable names (`reverie_create_window`,
`reverie_webview_open`, `reverie_run_loop`, ...). Each backend implements them
with its own windowing and web engine, deferring OS specifics to a C shim that
mirrors `shim/reverie_webview2.c`.

### Selecting a backend

`reverie/platform.reo` imports every backend. Each backend declares its API
inside an `@cfg(<os>) module`, so RingEcho's conditional compilation keeps only
the host's declarations and drops the rest before semantic analysis:

    @cfg(windows) module reverie_windows { ... }
    @cfg(macos)   module reverie_macos   { ... }
    @cfg(linux)   module reverie_linux   { ... }

This replaced generating a single-import `platform.reo` per target. macOS still
needs its C shim (Cocoa/WKWebView) before it can link; the Linux shim exists (see
below).

### Linux backend

`shim/linux/reverie_gtk.c` implements the `reverie_linux_*` ABI over GTK 3 and
WebKit2GTK. Because WebKit cannot intercept `https://reverie.local`, the shim
rewrites that URL to the internal `reverie://` scheme and serves files from the
mapped asset folder via a registered URI-scheme handler. Page-to-native messages
arrive through `window.webkit.messageHandlers.reverie`; native-to-page messages
call `window.__reverieReceive(...)`. The typed bridge in
`packages/create-reverie/templates/_base/src/reverie.ts` targets both this and
WebView2.

Build with:

    shim/linux/build.sh            # -> shim/linux/build/libreverie_gtk.a
    scripts/build-app.sh           # -> src-reverie/target/release/app

macOS and Linux packaging scripts live in `scripts/packager/`.

## Backends

| Backend | Windowing | Web engine | Status |
| --- | --- | --- | --- |
| Windows | Win32 (`user32`) | WebView2 (COM, `WebView2Loader.dll`) | ✅ M0 + M1 |
| macOS | Cocoa / AppKit | WKWebView | skeleton (`.reo` API in place; C shim TODO) |
| Linux | GTK 3 | WebKit2GTK 4.1 (4.0 fallback) | shim written (`shim/linux/`, untested on a real host) |

Cross-compiling and testing macOS/Linux requires those hosts (and their system
libraries); CI must build each on its own runner, as RingEcho already does.

## Tauri parity checklist

Runtime features, roughly in dependency order:

- [x] Win32 window + message loop
- [x] WebView2 host
- [x] Asset protocol (serve the bundled frontend; virtual host mapping on Windows)
- [x] IPC bridge (post message / poll on Windows; invoke/events/channels later)
- [x] Config file + schema (`reverie.conf` → generated `config.reo`)
- [x] Permissions / capabilities model (nav allowlist, CSP, command allowlist)
- [x] Multiple windows, DPI, window options
- [x] Menus, tray, native dialogs (open/save)
- [x] Dev-server integration and HMR
- [x] Bundling: one-command build (`scripts/build-app.ps1`) with runtime DLLs
- [~] Installers and resource embedding (Windows: Inno `app-setup.exe`, MSIX `app.msix`; macOS/Linux scripts in `scripts/packager/`)
- [ ] Auto-update and code signing
- [x] Logging (`reverie.log`); telemetry not planned
- [~] macOS and Linux backends (Linux shim written, untested; macOS `.reo` API in place, C shim pending)

## RingEcho prerequisites

Tracked in the RingEcho `ROADMAP.md`. Remaining for parity:

- [x] T3 `int ⇄ fn` cast (dynamic dispatch)
- [ ] L1 capturing closures (stateful IPC callbacks)
- [ ] L2 global/static state (runtime singletons)
- [ ] L3 Windows threads/async (event loop, async I/O)
- [x] L7 `@cfg` conditional compilation (backend selection above)
- [x] L5 `const` declarations
- [ ] L4 debug info, L6 namespaces
