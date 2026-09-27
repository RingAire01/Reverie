# Reverie roadmap

A desktop runtime for RingEcho, in the spirit of Tauri. Reverie is written in
`.reo` and binds to the OS through `extern C`; it owns the runtime orchestration
and leaves windowing and the web engine to the platform.

Legend: ✅ done · 🚧 in progress · ⛔ blocked · ⬜ not started

## Milestones

| # | Milestone | Status |
| --- | --- | --- |
| M0 | Win32 window + message loop, `WNDPROC` written in `.reo` | ✅ |
| M1 | WebView2 host via a thin C shim (WebView2Loader + COM) | ✅ |
| M2 | IPC bridge (JS ↔ `.reo`) and a `reverie://` asset protocol | ⬜ |
| M3 | Packaging, including the runtime DLLs | ⬜ |
| M4 | macOS (WKWebView) and Linux (WebKitGTK) backends | ⬜ |
| M5 | Ecosystem: `reverie` CLI, permissions, updater, signing | ⬜ |

## Gap analysis

### A. RingEcho language / toolchain

These are prerequisites; M1 onwards cannot proceed without them. Each was
confirmed by experiment against `rev v0.2.0` on Windows 11.

| # | Gap | Evidence | Needed for |
| --- | --- | --- | --- |
| A1 | No C `#include` channel | `@`-attributes only handle `@gc` / `@repr(C)` (`backend/codegen.c`) | M1 (WebView2 headers) |
| A2 | No link-flag channel | `rev run/build` accepts only `--shared/--target/--backend/--emit/-o`; `re0_build_compile` args are fixed | M1 (shim / WebView2Loader) |
| A3 | No `int → fn` cast | `invalid cast from 'u64' to 'fn'` | M1 alt (dlopen-style dispatch) |
| A4 | No capturing closures | README: "closures (no capture yet)" | M2 (callbacks with state) |
| A5 | No global/static state | no globals in the language | M2 (runtime singletons, registries) |
| A6 | Windows threads/async | `spawn/await` is pthread; Win32 thread model untested | M2/M5 (event loop, async I/O) |
| A7 | No debug info | no DWARF/PDB emission | all (debuggability) |
| A8 | Misc | no `const`; no namespaces (ABI names can't be prefixed); `rev check` does not load imports; `module` is reserved | ergonomics |

**Smallest unblock for M1:** add repeatable `--include <h>`, `--lib-dir <d>`,
`--link <l>` to `rev run/build`, forwarded to gcc as `-include`, `-L`, `-l`
(A1 + A2). A3 is a viable alternative but less general.

### B. Runtime features (Tauri parity)

- WebView2 host (M1)
- macOS WKWebView and Linux WebKitGTK backends (M4)
- IPC bridge: invoke/command, events, channels (M2)
- `reverie://` asset protocol and CSP (M2)
- Window management: multiple windows, sizing, decorations, transparency,
  fullscreen, DPI
- Menus, tray, native dialogs (file open/save)
- Permissions / capabilities model (Tauri ACL)
- Config file + JSON schema (`reverie.conf`)
- Auto-update and code signing
- Bundling: installers, resource embedding
- Dev-server integration and HMR
- State-management API
- Logging / telemetry

### C. Tooling / CLI

- A `reverie` CLI: `dev`, `build`, `bundle` (today only `create-reverie` exists)
- npm publishing (blocked locally: npm not authenticated)
- More templates: TypeScript variants, Svelte, Solid
- Icons / branding assets

### D. Infrastructure

- CI for Reverie (currently verified by hand with `rev run`)
- Automated runtime tests (today only manual runs)
- Release pipeline (npm + GitHub Releases)
- Versioning / ABI-compatibility policy

## Minimal usable MVP

The smallest set that makes Reverie genuinely useful:

1. ~~**M1** WebView2 host~~ ✅
2. IPC bridge (needs A4 + A5)
3. `reverie://` asset protocol
4. Dev-server integration (Vite / Next)
5. Packaging that bundles the runtime DLLs

Everything else (M4, M5) can follow once that path is proven on Windows.

## Current state

M0 + M1. The native side opens a Win32 window and hosts a WebView2 that
navigates to a URL; the COM details live in a C shim (`shim/reverie_webview2.c`)
linked with `rev build --lib-dir shim --link reverie_webview2 --link ole32`.

Verified on Windows 11: `reverie_webview_open` dispatches, the controller is
created, and `reverie_webview_state()` reaches `3` (navigated) with no error.

Still missing before it is usable: the frontend (from `src/`) is not yet served
into the webview, there is no IPC bridge, and packaging must bundle
`WebView2Loader.dll` (and `libwinpthread-1.dll`, or link statically).
