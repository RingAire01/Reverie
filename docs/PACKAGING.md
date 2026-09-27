# Reverie Packager

Produces distributable bundles for a Reverie app. A release build first
(`scripts/build-app.ps1`), then one bundle per requested format.

```
scripts/
  package-app.ps1        Windows orchestrator (inno, msix)
  package-app.sh         macOS/Linux orchestrator
  packager/
    inno.ps1             Windows: Inno Setup installer (.exe)
    msix.ps1             Windows: MSIX (.msix, or a signed layout)
    macos.sh             macOS: .app bundle + .dmg
    linux.sh             Linux: AppImage + .deb + .rpm
```

## Formats by platform

| Platform | Format | Produced by | Host tooling |
| --- | --- | --- | --- |
| Windows | Inno Setup `.exe` | `packager/inno.ps1` | Inno Setup 6 |
| Windows | `.msix` | `packager/msix.ps1` | Windows SDK (`makeappx`, `signtool`) + cert |
| macOS | `.app` | `packager/macos.sh` | executed on macOS |
| macOS | `.dmg` | `packager/macos.sh` | `hdiutil` (macOS) |
| Linux | AppImage | `packager/linux.sh` | `appimagetool` |
| Linux | `.deb` | `packager/linux.sh` | `dpkg-deb` |
| Linux | `.rpm` | `packager/linux.sh` | `rpmbuild` |

Each bundle ships the executable, its runtime libraries, and the built frontend
(`dist/`). The runtime resolves the asset folder relative to the executable, so
the same build works from any install location.

## Windows

```powershell
powershell -File scripts/package-app.ps1 -Name app -Version 0.1.0
# or a single format:
powershell -File scripts/package-app.ps1 -Name app -Formats inno
```

- `inno` installs per-user to `%LOCALAPPDATA%\Programs\<name>`.
- `msix` writes a layout under `src-reverie/target/msix/layout`; with the Windows
  SDK it packs `makeappx` and, given `-CertPath`, signs with `signtool`.

## macOS / Linux

```bash
NAME=app VERSION=0.1.0 scripts/package-app.sh
```

Bundlers are per-host: they assume the matching Reo backend produced the native
binary under `src-reverie/target/release/`. Formats whose tooling is missing are
skipped with a message.

## Status

- Windows: Inno Setup verified end to end; MSIX produces the layout (packing
  requires the Windows SDK).
- macOS / Linux: scripts are in place; they require the respective Reo backend
  (`platform/macos`, `platform/linux`) and host tooling, which are not yet
  implemented (see [PLATFORMS.md](PLATFORMS.md)).
