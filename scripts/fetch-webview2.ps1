# Fetch the WebView2 SDK (headers + WebView2Loader.dll) into shim/webview2/.
# The SDK is not committed; this makes the build reproducible.
param(
    [string]$Version = '1.0.4191.47'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$dest = Join-Path $root 'shim/webview2'

$zip = Join-Path $env:TEMP "webview2-$Version.zip"
$url = "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$Version/microsoft.web.webview2.$Version.nupkg"
Write-Host "downloading $url"
if (-not (Test-Path $zip)) {
    & curl.exe -L --ssl-no-revoke -C - --max-time 600 -o $zip $url
    if ($LASTEXITCODE -ne 0) { throw 'download failed' }
}

$pkg = Join-Path $env:TEMP "webview2-$Version"
Remove-Item -LiteralPath $pkg -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath $zip -DestinationPath $pkg -Force

New-Item -ItemType Directory -Path (Join-Path $dest 'include'), (Join-Path $dest 'x64') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $pkg 'build/native/include/WebView2.h') -Destination (Join-Path $dest 'include/WebView2.h') -Force
Copy-Item -LiteralPath (Join-Path $pkg 'build/native/x64/WebView2Loader.dll') -Destination (Join-Path $dest 'x64/WebView2Loader.dll') -Force
Write-Host "installed WebView2 $Version into $dest"
