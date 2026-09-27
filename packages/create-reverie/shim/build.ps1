# Build the Reverie WebView2 shim into a static library.
#
#   $env:REO_CC = 'C:\...\llvm-mingw...\bin\gcc.exe'
#   powershell -File shim/build.ps1
#
# Artifacts go under shim/build/. Requires shim/webview2/ (headers +
# WebView2Loader.dll); fetch it with scripts/fetch-webview2.ps1.
param(
    [string]$CC = $env:REO_CC,
    [string]$Ar = $env:REO_AR
)
# Compiler stderr is not fatal; failures are checked via $LASTEXITCODE.
$ErrorActionPreference = 'Continue'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $CC) { $CC = 'gcc' }
if (-not $Ar) { $Ar = 'llvm-ar' }

$include = Join-Path $here 'webview2/include'
if (-not (Test-Path (Join-Path $include 'WebView2.h'))) {
    throw "missing WebView2.h; run scripts/fetch-webview2.ps1 first"
}

$out = Join-Path $here 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null

& $CC -c -O2 -I $include (Join-Path $here 'reverie_webview2.c') -o (Join-Path $out 'reverie_webview2.o')
if ($LASTEXITCODE -ne 0) { throw 'shim compile failed' }

& $Ar rcs (Join-Path $out 'libreverie_webview2.a') (Join-Path $out 'reverie_webview2.o')
if ($LASTEXITCODE -ne 0) { throw 'shim archive failed' }

Write-Host "built $(Join-Path $out 'libreverie_webview2.a')"
