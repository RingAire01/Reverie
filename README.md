# Reverie

A desktop runtime for RingEcho, in the spirit of Tauri.

Reverie is written in RingEcho (`.reo`) and binds to the operating system through
`extern C`. It does not reimplement windowing or the web engine; it owns the
runtime orchestration on top of the platform's C ABI.

## Status

Milestone **M0**: a Win32 window plus a message loop, driven from `.reo`.

- Platform: Windows only, C backend (`rev build` / `rev run`).

## Layout

```
src/
  reverie.reo              public facade (import this)
  reverie/
    abi/structs.reo        C-layout structs (WndClassA, Msg, Rect)
    abi/user32.reo         user32 externs
    abi/kernel32.reo       kernel32 externs
    messages.reo           WM_* identifiers
    window.reo             class registration, creation, default procedure
    loop.reo               message loop
  main.reo                 M0 demo, uses only the facade
```

RingEcho has no namespaces: every imported declaration lands in one global
scope. Imports resolve relative to the **entry file's** directory, so all
import paths are written from `src/`. Public API names carry a `reverie_`
prefix; the ABI layer keeps the exact Win32 names because they must match the C
symbols.

Later milestones:

| Milestone | Scope |
| --- | --- |
| M1 | WebView2 host via a thin C shim (WebView2Loader + COM) |
| M2 | IPC bridge (JS <-> `.reo` commands) and an asset protocol |
| M3 | Packaging, including the runtime DLLs |

## Build and run

Requires the `rev` compiler and a Windows C toolchain on `PATH` (for example
LLVM-MinGW, which also links `user32`/`kernel32` by default).

```sh
rev run src/main.reo
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
