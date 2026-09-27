# Reverie Packager (Windows): release build + one or more bundle formats.
#
#   powershell -File scripts/package-app.ps1 [-Name app] [-Version 0.1.0]
#                                            [-Formats inno,msix]
#
# Formats: inno (Inno Setup .exe), msix (.msix / layout).
# macOS (.app/.dmg) and Linux (AppImage/.deb/.rpm) are produced by
# scripts/package-app.sh on those hosts.
param(
    [string]$Name = "app",
    [string]$Version = "0.1.0",
    [string]$Publisher = "Reverie",
    [string[]]$Formats = @('inno', 'msix'),
    [string]$Rev = $env:REV,
    [string]$CC = $env:REO_CC,
    [string]$Ar = $env:REO_AR
)
$ErrorActionPreference = 'Continue'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$appRoot = Split-Path -Parent $here
$exe = Join-Path $appRoot "src-reverie/target/release/$Name.exe"

& (Join-Path $here 'build-app.ps1') -Name $Name -Profile release -Rev $Rev -CC $CC -Ar $Ar
if (-not (Test-Path $exe)) { throw 'release build failed' }

foreach ($fmt in $Formats) {
    $script = Join-Path $here "packager/$fmt.ps1"
    if (-not (Test-Path $script)) { Write-Warning "unknown format '$fmt'"; continue }
    & $script -Name $Name -Version $Version -Publisher $Publisher
}
