# Render every factory preset with kmi_render and encode the results to MP3 for docs/audio.
#   .\scripts\build.ps1                # first: builds kmi_render
#   .\scripts\make-audio-demos.ps1     # -> docs\audio\NN-preset-name.mp3
# Needs ffmpeg on PATH (for example: winget install Gyan.FFmpeg).
param(
    [string]$Config = "Release",
    [int]$Bitrate = 128
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$ffmpeg = (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source
if (-not $ffmpeg) { $ffmpeg = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\ffmpeg.exe" }
if (-not (Test-Path $ffmpeg)) { throw "ffmpeg not found. Install it (winget install Gyan.FFmpeg) and open a new shell." }

$render = Join-Path $root "build\tests\$Config\kmi_render.exe"
if (-not (Test-Path $render)) { throw "No kmi_render at $render. Run scripts\build.ps1 first." }

$wavDir = Join-Path ([System.IO.Path]::GetTempPath()) "kmi-demos"
if (Test-Path $wavDir) { Remove-Item -Recurse -Force $wavDir }
New-Item -ItemType Directory $wavDir | Out-Null
& $render $wavDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$out = Join-Path $root "docs\audio"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter *.mp3 | Remove-Item
foreach ($wav in Get-ChildItem $wavDir -Filter *.wav | Sort-Object Name)
{
    # "01 Velvet Tines.wav" -> "01-velvet-tines.mp3"
    $slug = ($wav.BaseName.ToLowerInvariant() -replace '[^a-z0-9]+', '-').Trim('-')
    & $ffmpeg -hide_banner -loglevel error -y -i $wav.FullName -codec:a libmp3lame -b:a "${Bitrate}k" `
        -metadata title="$($wav.BaseName.Substring(3)) (KMISynth factory preset)" -metadata artist="KMISynth" (Join-Path $out "$slug.mp3")
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed on $($wav.Name)" }
}
$files = Get-ChildItem $out -Filter *.mp3
Write-Host ("{0} demos, {1:N1} MB in {2}" -f $files.Count, (($files | Measure-Object Length -Sum).Sum / 1MB), $out)
