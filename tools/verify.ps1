# The CI gate on Windows, the twin of tools/verify.sh: build every target,
# then the unit tests, the FX plugin tests, the UI test, the preset
# fingerprints and pluginval at strictness 10 on both plugins. Stops at the
# first failure. The self-hosted runner workflow (.github/workflows/
# self-hosted.yml) runs this; it works the same from a terminal.
#
#   powershell -ExecutionPolicy Bypass -File tools\verify.ps1            everything
#   powershell -ExecutionPolicy Bypass -File tools\verify.ps1 -Quick     skip pluginval
#   ... -Jobs 2                                                           build parallelism
#
# Jobs defaults to 1 (one project at a time): this PC runs out of memory
# with more. Needs CMake and Visual Studio 2022; Python for the fingerprint
# check (skipped without it). pluginval is fetched once into build\pluginval.
param(
    [switch] $Quick,
    [int] $Jobs = 1
)

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
$env:ILANA_CI = "1"

function Step($name) { Write-Host ""; Write-Host "=== $name" }
function Check($what) { if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" } }

$release = "build\{0}_artefacts\Release\{0}.exe"

Step "Build"
if (-not (Test-Path build\CMakeCache.txt)) {
    cmake -B build -G "Visual Studio 17 2022" -A x64; Check "configure"
}
cmake --build build --config Release --parallel $Jobs; Check "build"

Step "Unit tests"
& ($release -f "ilanaTableTest") | Tee-Object -FilePath build\table-test.txt | Select-String -Pattern "^FAIL|TESTS"
if (-not (Select-String -Path build\table-test.txt -Pattern "ALL TESTS PASSED" -Quiet)) { throw "unit tests failed (build\table-test.txt)" }

Step "FX plugin tests"
& ($release -f "ilanaFxTest") | Select-Object -Last 1; Check "FX plugin tests"

Step "UI tests"
& ($release -f "ilanaSnapshot") --uitest | Select-Object -Last 1; Check "UI tests"

Step "Preset fingerprints"
& ($release -f "ilanaFingerprint") build\fingerprints.csv | Out-Null; Check "fingerprints"
# Fingerprints differ in the last bits between platforms: compare against a
# Windows baseline when one is committed (copy build\fingerprints.csv to
# tests\fingerprints-windows.csv to make one).
$python = Get-Command python -ErrorAction SilentlyContinue
if ((Test-Path tests\fingerprints-windows.csv) -and $python) {
    python tools\compare_fingerprints.py tests\fingerprints-windows.csv build\fingerprints.csv --fail | Select-Object -Last 3
    Check "fingerprint comparison"
} else {
    Write-Host "written to build\fingerprints.csv (no Windows baseline or no Python: not compared)"
}

if (-not $Quick) {
    Step "pluginval (strictness 10)"
    $pv = if ($env:PLUGINVAL) { $env:PLUGINVAL } else { "build\pluginval\pluginval.exe" }
    if (-not (Test-Path $pv)) {
        New-Item -ItemType Directory -Force build\pluginval | Out-Null
        Invoke-WebRequest -Uri https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip -OutFile build\pluginval\pluginval.zip
        Expand-Archive -Force build\pluginval\pluginval.zip build\pluginval
    }
    foreach ($plugin in @("build\ilanaSynth_artefacts\Release\VST3\ilanaSynth.vst3", "build\ilanaSynthFX_artefacts\Release\VST3\ilanaSynth FX.vst3")) {
        & $pv --strictness-level 10 --validate-in-process --timeout-ms 600000 $plugin *> build\pluginval.txt
        if ($LASTEXITCODE -ne 0) { Get-Content build\pluginval.txt -Tail 30; throw "pluginval failed on $plugin" }
        Write-Host "${plugin}: $(Select-String -Path build\pluginval.txt -Pattern 'SUCCESS|FAILED' | Select-Object -Last 1)"
    }
}

Step "All checks passed"
