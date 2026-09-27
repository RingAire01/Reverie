# Build the Reverie WebView2 shim into a static library.
#
#   $env:REO_CC = 'C:\...\llvm-mingw...\bin\gcc.exe'
#   pwsh shim/build.ps1
#
# Requires shim/webview2/ (headers + WebView2Loader.dll); fetch it with
# scripts/fetch-webview2.ps1.
param(
    [string]$CC = $env:REO_CC,
    [string]$Ar = $env:REO_AR
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $CC) { $CC = 'gcc' }
if (-not $Ar) { $Ar = 'llvm-ar' }

$include = Join-Path $here 'webview2/include'
if (-not (Test-Path (Join-Path $include 'WebView2.h'))) {
    throw "missing WebView2.h; run scripts/fetch-webview2.ps1 first"
}

& $CC -c -O2 -I $include (Join-Path $here 'reverie_webview2.c') -o (Join-Path $here 'reverie_webview2.o')
if ($LASTEXITCODE -ne 0) { throw 'shim compile failed' }

& $Ar rcs (Join-Path $here 'libreverie_webview2.a') (Join-Path $here 'reverie_webview2.o')
if ($LASTEXITCODE -ne 0) { throw 'shim archive failed' }

Write-Host "built $(Join-Path $here 'libreverie_webview2.a')"
