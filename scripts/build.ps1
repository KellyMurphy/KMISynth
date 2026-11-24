# Configure + build KMISynth with the CMake bundled in Visual Studio 2022.
#   .\scripts\build.ps1                 # Release, plugin + tests
#   .\scripts\build.ps1 -Config Debug
#   .\scripts\build.ps1 -EngineOnly     # skip JUCE / plugin (fast DSP iteration)
#   .\scripts\build.ps1 -Test           # also run the engine tests
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")] [string]$Config = "Release",
    [switch]$EngineOnly,
    [switch]$Test
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
    $cmake = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}

$buildDir = if ($EngineOnly) { Join-Path $root "build-engine" } else { Join-Path $root "build" }
$plugin = if ($EngineOnly) { "OFF" } else { "ON" }

& $cmake -S $root -B $buildDir -G "Visual Studio 17 2022" -A x64 "-DKMI_BUILD_PLUGIN=$plugin"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $buildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Test) {
    & (Join-Path $buildDir "tests\$Config\kmi_engine_tests.exe")
    exit $LASTEXITCODE
}

if (-not $EngineOnly) {
    Write-Host "`nVST3:       $buildDir\plugin\KMISynth_artefacts\$Config\VST3\KMISynth.vst3"
    Write-Host "Standalone: $buildDir\plugin\KMISynth_artefacts\$Config\Standalone\KMISynth.exe"
}
