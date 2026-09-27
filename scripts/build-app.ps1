# Build a Reverie app end to end: shim -> native binary -> runtime DLLs.
#
#   pwsh scripts/build-app.ps1 [-Name app]
#
# Requires `rev` and a Windows C toolchain on PATH (for example LLVM-MinGW).
# The WebView2 SDK is fetched into shim/webview2/ on first run.
param(
    [string]$Name = "app",
    [string]$Rev = $env:REV,
    [string]$CC = $env:REO_CC,
    [string]$Ar = $env:REO_AR
)
$ErrorActionPreference = 'Stop'

if (-not $Rev) { $Rev = 'rev' }
if (-not $CC) { $CC = 'gcc' }
if (-not $Ar) { $Ar = 'llvm-ar' }

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$appRoot = Split-Path -Parent $here
$shim = Join-Path $appRoot 'shim'
$srcReve = Join-Path $appRoot 'src-reverie'
$out = Join-Path $appRoot "$Name.exe"

# 1. WebView2 SDK (headers + loader DLL), fetched once.
if (-not (Test-Path (Join-Path $shim 'webview2/include/WebView2.h'))) {
    & (Join-Path $here 'fetch-webview2.ps1')
}

# 2. C shim static library.
& (Join-Path $shim 'build.ps1') -CC $CC -Ar $Ar

# 3. Native binary (RingEcho runtime).
& $Rev build (Join-Path $srcReve 'main.reo') --lib-dir $shim --link reverie_webview2 --link ole32 -o $out
if ($LASTEXITCODE -ne 0) { throw 'rev build failed' }

# 4. Runtime DLLs next to the executable.
Copy-Item -LiteralPath (Join-Path $shim 'webview2/x64/WebView2Loader.dll') -Destination (Join-Path $appRoot 'WebView2Loader.dll') -Force
$ccDir = Split-Path -Parent (Get-Command $CC).Source
$winpthread = Join-Path $ccDir 'libwinpthread-1.dll'
if (Test-Path $winpthread) {
    Copy-Item -LiteralPath $winpthread -Destination (Join-Path $appRoot 'libwinpthread-1.dll') -Force
}

Write-Host "built $out"
