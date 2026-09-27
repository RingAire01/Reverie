# Reverie

A desktop runtime for RingEcho, in the spirit of Tauri.

Reverie is written in RingEcho (`.reo`) and binds to the operating system through
`extern C`. It does not reimplement windowing or the web engine; it owns the
runtime orchestration on top of the platform's C ABI.

## Status

Milestones **M0** and **M1** on Windows:

- M0: a Win32 window plus a message loop, driven from `.reo`.
- M1: a WebView2 hosted in that window. The COM details live in
  `shim/reverie_webview2.c`; the Reo side calls a plain C API.
- M2: an asset protocol (`reverie_asset_folder` maps a folder to a virtual host)
  and an IPC bridge (`reverie_send` / `reverie_poll`), so a built frontend runs
  without a dev server.

- Platform: Windows, C backend (`rev build` / `rev run`).

## Layout

```
src/
  reverie.reo              public facade (import this)
  reverie/
    platform.reo           host backend selection (single import point)
    platform/windows/      Win32 + WebView2 backend
      abi/*.reo            C-layout structs and user32/kernel32 externs
      messages.reo  window.reo  loop.reo  webview.reo
  main.reo                 M1 demo (window + WebView2)
shim/
  reverie_webview2.c/.h    C boundary over the WebView2 COM API
  build.ps1                compiles shim/libreverie_webview2.a
  webview2/                fetched SDK (not committed)
```

The runtime is split into a platform-agnostic facade and per-OS backends;
see [docs/PLATFORMS.md](docs/PLATFORMS.md) for the architecture and the Tauri parity
checklist. RingEcho has no namespaces: every imported declaration lands in one
global scope, and imports resolve relative to the **entry file's** directory, so
all import paths are written from `src/`. Public API names carry a `reverie_`
prefix; the ABI layer keeps the exact Win32 names because they must match the C
symbols.

See [docs/ROADMAP.md](docs/ROADMAP.md) for the full milestone plan and the gap analysis
(language prerequisites, runtime features, tooling, infrastructure).

## Scaffolding

`packages/create-reverie` is the Node initializer. It produces the Tauri-style
split so Reverie fits JS/TS tooling: a web frontend in `src/` and the RingEcho
runtime in `src-reverie/`.

```sh
pnpm create @ringaire/reverie my-app --template react
# templates: vanilla, react, vue, next (react/vue/next are TypeScript)
```

TypeScript apps get a typed IPC bridge (`src/reverie.ts`): `invoke(cmd, arg)` to
call native commands and `onMessage(handler)` for replies.

Each generated app is self-contained: it ships the RingEcho runtime
(`src-reverie/`), the C shim (`shim/`) and the build scripts (`scripts/`).

```sh
pnpm install
pnpm reverie:build   # shim + native binary + runtime DLLs
pnpm reverie:dev     # build, then launch app.exe
```

`reverie:build` needs `rev` and a Windows C toolchain on `PATH`; the WebView2
SDK is fetched into `shim/webview2/` on first run.

Output mirrors Tauri's `src-tauri/target` layout — nothing is written to the app
root:

```
src-reverie/target/release/
  app.exe
  WebView2Loader.dll
  libwinpthread-1.dll
shim/build/
  libreverie_webview2.a
```

## Build and run

Requires the `rev` compiler and a Windows C toolchain on `PATH` (for example
LLVM-MinGW, which also links `user32`/`kernel32` by default).

M0 only (no WebView2):

```sh
rev run src/main.reo
```

### WebView2 (M1)

Requires a `rev` built from RingEcho source that includes the host-integration
flags `--include` / `--lib-dir` / `--link` (RingEcho commit `5f11634` or newer;
an older installed `rev` reports `unknown option: --lib-dir`).

```sh
powershell -File scripts/fetch-webview2.ps1   # once: WebView2 SDK into shim/webview2/
powershell -File shim/build.ps1               # build shim/build/libreverie_webview2.a
rev build src/main.reo --lib-dir shim/build --link reverie_webview2 --link ole32 -o target/release/app.exe
```

Place `WebView2Loader.dll` (from `shim/webview2/x64/`) and `libwinpthread-1.dll`
next to `target/release/app.exe`. The executables under `target/` are build
artifacts and are not committed. `app.exe` prints the state and exits 0:

```
1        IsWindow(hwnd)
0        reverie_webview_open dispatched
3        WebView2 state: navigated
Reverie M1 finished
```

The window procedure (`wnd_proc`) is written in RingEcho. RingEcho lowers
function types to bare C function pointers, so a plain `.reo` function can be
registered as the Win32 `WNDPROC` directly.

Verified on Windows 11 / LLVM-MinGW 22.1.8 (`rev v0.2.0`):

```
50115                 # RegisterClassA atom (non-zero: class registered)
1                     # IsWindow(hwnd)
640                   # window width
480                   # window height
Reverie M0 finished   # message loop ended, process exited 0
```

A window titled `Reverie M0` appears for about two seconds; `SetTimer` posts
`WM_TIMER`, the procedure closes the window, and the loop exits cleanly.

## Known constraints

RingEcho has no capturing closures yet, so callbacks are limited to plain
functions with no captured environment. M0 relies on this directly; M2's JS
<-> `.reo` command bridge will need a documented trampoline workaround.
