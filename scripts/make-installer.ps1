# Build the Windows installer (Inno Setup 6) from a Release build.
#   .\scripts\build.ps1            # first: VST3 + Standalone
#   .\scripts\make-installer.ps1   # -> build\installer\KMISynth-<version>-Windows-Setup.exe
param(
    [string]$Iscc = "",
    [string]$Config = "Release"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if (-not $Iscc) {
    $candidates = @(
        (Get-Command iscc -ErrorAction SilentlyContinue).Source,
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    ) | Where-Object { $_ -and (Test-Path $_) }
    $Iscc = $candidates | Select-Object -First 1
}
if (-not $Iscc) { throw "Inno Setup 6 (ISCC.exe) not found. Install it from https://jrsoftware.org/isdl.php or pass -Iscc." }

# The version comes from the CMake project() line.
$cmakeLists = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cmakeLists -notmatch 'project\(KMISynth VERSION ([0-9.]+)') { throw "Could not read the project version." }
$version = $Matches[1]

$source = Join-Path $root "build\plugin\KMISynth_artefacts\$Config"
if (-not (Test-Path (Join-Path $source "VST3\KMISynth.vst3"))) { throw "No $Config build at $source. Run scripts\build.ps1 first." }
$out = Join-Path $root "build\installer"

& $Iscc "/DAppVersion=$version" "/DSourceDir=$source" "/DOutputDir=$out" (Join-Path $root "installer\KMISynth.iss")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "`nInstaller: $out\KMISynth-$version-Windows-Setup.exe"
