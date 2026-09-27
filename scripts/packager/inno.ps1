# Windows / Inno Setup installer (.exe). Requires Inno Setup 6 (ISCC.exe).
# Expects src-reverie/target/release/<Name>.exe to be built already.
param(
    [string]$Name = "app",
    [string]$Version = "0.1.0",
    [string]$Publisher = "Reverie"
)
$ErrorActionPreference = 'Continue'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$appRoot = Split-Path -Parent (Split-Path -Parent $here)
$outDir = Join-Path $appRoot 'src-reverie/target/release'

$candidates = @(
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
)
$iscc = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup 6 (ISCC.exe) not found; install it from https://jrsoftware.org/isdl.php' }

$issPath = Join-Path $outDir "$Name.iss"
$iss = @"
[Setup]
AppName=$Name
AppVersion=$Version
AppPublisher=$Publisher
DefaultDirName={localappdata}\Programs\$Name
DefaultGroupName=$Name
OutputDir=$outDir
OutputBaseFilename=$Name-setup
PrivilegesRequired=lowest
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\$Name.exe

[Files]
Source: "$outDir\$Name.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "$outDir\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "$outDir\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "$outDir\dist\*"; DestDir: "{app}\dist"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\$Name"; Filename: "{app}\$Name.exe"
Name: "{group}\Uninstall $Name"; Filename: "{uninstallexe}"
"@
[System.IO.File]::WriteAllText($issPath, $iss, (New-Object System.Text.UTF8Encoding($false)))

& $iscc $issPath
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compile failed' }
Write-Host "packaged (inno): $(Join-Path $outDir "$Name-setup.exe")"
