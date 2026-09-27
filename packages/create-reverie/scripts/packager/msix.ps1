# Windows / MSIX (.msix). Generates the package layout + AppxManifest.xml and,
# when the Windows SDK's makeappx.exe is available, packs and signs it.
#
# Signing needs a code-signing certificate; set -CertPath to a .pfx, otherwise
# the layout is produced unsigned (open-package / re-sign later).
param(
    [string]$Name = "app",
    [string]$Version = "0.1.0",
    [string]$Publisher = "CN=Reverie",
    [string]$IdentityName = "Reverie.App",
    [string]$CertPath = "",
    [string]$CertPassword = ""
)
$ErrorActionPreference = 'Continue'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$appRoot = Split-Path -Parent (Split-Path -Parent $here)
$srcReve = Join-Path $appRoot 'src-reverie'
$outDir = Join-Path $srcReve 'target/release'
$layout = Join-Path $srcReve 'target/msix/layout'
$assets = Join-Path $layout 'Assets'

if (-not (Test-Path (Join-Path $outDir "$Name.exe"))) { throw "$Name.exe not built; run build-app.ps1 first" }

Remove-Item -LiteralPath $layout -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $assets -Force | Out-Null

# 1. Payload.
Copy-Item -LiteralPath (Join-Path $outDir "$Name.exe") -Destination $layout -Force
foreach ($dll in 'WebView2Loader.dll', 'libwinpthread-1.dll') {
    $from = Join-Path $outDir $dll
    if (Test-Path $from) { Copy-Item -LiteralPath $from -Destination $layout -Force }
}
$dist = Join-Path $outDir 'dist'
if (Test-Path $dist) { Copy-Item -LiteralPath $dist -Destination (Join-Path $layout 'dist') -Recurse -Force }

# 2. Placeholder logo assets (replace with real branding).
Add-Type -AssemblyName System.Drawing
function New-Png([string]$path, [int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::FromArgb(32, 33, 36))
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}
New-Png (Join-Path $assets 'Square44x44Logo.png') 44 44
New-Png (Join-Path $assets 'Square150x150Logo.png') 150 150
New-Png (Join-Path $assets 'Wide310x150Logo.png') 310 150
New-Png (Join-Path $assets 'StoreLogo.png') 50 50

# 3. AppxManifest.xml (full-trust Win32 app).
$version4 = ($Version -replace '-.*$', '') + '.0'
$manifest = @"
<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
         xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities">
  <Identity Name="$IdentityName" Publisher="$Publisher" Version="$version4" />
  <Properties>
    <DisplayName>$Name</DisplayName>
    <PublisherDisplayName>$Publisher</PublisherDisplayName>
    <Logo>Assets\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.19041.0" MaxVersionTested="10.0.22621.0" />
  </Dependencies>
  <Resources><Resource Language="en-us" /></Resources>
  <Applications>
    <Application Id="App" Executable="$Name.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="$Name" Description="$Name" BackgroundColor="transparent"
        Square150x150Logo="Assets\Square150x150Logo.png" Square44x44Logo="Assets\Square44x44Logo.png" />
    </Application>
  </Applications>
  <Capabilities>
    <rescap:Capability Name="runFullTrust" />
  </Capabilities>
</Package>
"@
[System.IO.File]::WriteAllText((Join-Path $layout 'AppxManifest.xml'), $manifest, (New-Object System.Text.UTF8Encoding($false)))

# 4. Pack with makeappx if present.
$makeappx = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin", "$env:ProgramFiles\Windows Kits\10\bin" -Recurse -Filter makeappx.exe -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $makeappx) {
    Write-Host "makeappx.exe (Windows SDK) not found; MSIX layout written to $layout"
    return
}
$msix = Join-Path $outDir "$Name.msix"
& $makeappx.FullName pack /o /d $layout /p $msix
if ($LASTEXITCODE -ne 0) { throw 'makeappx pack failed' }
Write-Host "packaged (msix): $msix"

if ($CertPath) {
    $signtool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin", "$env:ProgramFiles\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $signtool) { Write-Warning 'signtool.exe not found; MSIX left unsigned'; return }
    & $signtool.FullName sign /fd SHA256 /a /f $CertPath /p $CertPassword $msix
    if ($LASTEXITCODE -ne 0) { throw 'sign failed' }
    Write-Host "signed: $msix"
}
