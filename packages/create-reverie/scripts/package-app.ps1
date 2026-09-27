# Build a Reverie app and wrap it in an Inno Setup installer.
#
#   powershell -File scripts/package-app.ps1 [-Name app] [-Version 0.1.0]
#
# Produces src-reverie/target/release/<Name>-setup.exe, which installs the
# executable, the runtime DLLs and the bundled frontend (dist/) per-user.
#
# Requires Inno Setup 6 (ISCC.exe).
param(
    [string]$Name = "app",
    [string]$Version = "0.1.0",
    [string]$Publisher = "Reverie",
    [string]$Rev = $env:REV,
    [string]$CC = $env:REO_CC,
    [string]$Ar = $env:REO_AR
)
$ErrorActionPreference = 'Continue'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$appRoot = Split-Path -Parent $here
$srcReve = Join-Path $appRoot 'src-reverie'
$outDir = Join-Path $srcReve 'target/release'

# 1. Release build (exe + DLLs + dist).
& (Join-Path $here 'build-app.ps1') -Name $Name -Profile release -Rev $Rev -CC $CC -Ar $Ar
if (-not (Test-Path (Join-Path $outDir "$Name.exe"))) { throw 'release build failed' }

# 2. Locate the Inno Setup compiler.
$candidates = @(
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
)
$iscc = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup 6 (ISCC.exe) not found; install it from https://jrsoftware.org/isdl.php' }

# 3. Generate the installer script.
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

# 4. Compile the installer.
& $iscc $issPath
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compile failed' }

Write-Host "installer: $(Join-Path $outDir "$Name-setup.exe")"
