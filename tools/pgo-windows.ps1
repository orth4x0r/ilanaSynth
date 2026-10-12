# A profile-guided (PGO) build of the ilanaSynth VST3 with Visual Studio:
# builds a plugin that counts which code runs, plays every preset through it
# (ilanaRefHost --train: a pedalled run, a chord, fast repeats), then builds
# again with the compiler laying the code out for what it saw. Its own build
# folder (build-pgo), so the normal build is left alone. Optional: the plain
# build is fine; this one is a few percent lighter on CPU.
#
#   powershell -ExecutionPolicy Bypass -File tools\pgo-windows.ps1
#   ... -Presets "D:\more presets"        folders to train on (default: Documents\ilanaSynth Presets)
#   ... -InstallTo "C:\path\to\VST3"      copy the finished plugin there
#   ... -Jobs 2                           build parallelism (default 1: this PC runs out of memory with more)
#
# Needs CMake and Visual Studio 2022 (the C++ workload), like tools\verify.ps1.
param(
    [string[]] $Presets = @((Join-Path ([Environment]::GetFolderPath("MyDocuments")) "ilanaSynth Presets")),
    [string] $InstallTo = "",
    [int] $Jobs = 1
)

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

function Step($name) { Write-Host ""; Write-Host "=== $name" }
function Check($what) { if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" } }

$build = "build-pgo"
$vst3 = "$build\ilanaSynth_artefacts\Release\VST3\ilanaSynth.vst3"
$refHost = "$build\ilanaRefHost_artefacts\Release\ilanaRefHost.exe"
$pgoFolder = "$build\pgo"

# The counting plugin needs Visual Studio's PGO runtime (pgort140.dll) on
# PATH, both for the build's own plugin check and for the training run.
Step "Find the PGO runtime"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found: is Visual Studio 2022 installed?" }
$runtime = & $vswhere -latest -products * -find "VC\Tools\MSVC\**\bin\Hostx64\x64\pgort140.dll" | Select-Object -First 1
if (-not $runtime) { throw "pgort140.dll not found in Visual Studio (the C++ desktop workload installs it)" }
$env:PATH = (Split-Path $runtime) + ";" + $env:PATH
Write-Host $runtime

Step "Build the counting plugin"
cmake -B $build -G "Visual Studio 17 2022" -A x64 -DILANA_PGO=GEN; Check "configure"
cmake --build $build --config Release --parallel $Jobs --target ilanaSynth_VST3 ilanaRefHost; Check "build"

Step "Train"
# Counts from the build's own plugin check (it loads the plugin) don't
# belong in the profile.
Get-ChildItem -Recurse -Filter *.pgc $build | Remove-Item
$folders = @($Presets | Where-Object { Test-Path $_ })
if ($folders.Count -eq 0) { Write-Host "no preset folder found ($($Presets -join ', ')): training on the init sound only" }
& $refHost --train $vst3 @folders; Check "training run"
# The counts are written next to the plugin's .dll or next to the profile,
# depending on the Visual Studio version: gather them beside the profile.
Get-ChildItem -Recurse -Filter *.pgc $build | Where-Object { $_.DirectoryName -ne (Resolve-Path $pgoFolder).Path } |
    Move-Item -Force -Destination $pgoFolder
$counts = @(Get-ChildItem -Filter *.pgc $pgoFolder)
if ($counts.Count -eq 0) { throw "the training run wrote no .pgc files" }
Write-Host "$($counts.Count) count file(s) in $pgoFolder"

Step "Build the optimised plugin"
cmake -B $build -DILANA_PGO=USE; Check "configure"
cmake --build $build --config Release --parallel $Jobs --target ilanaSynth_VST3; Check "build"

if ($InstallTo) {
    Step "Install"
    $target = Join-Path $InstallTo "ilanaSynth.vst3"
    if (Test-Path $target) { Remove-Item -Recurse -Force $target }
    Copy-Item -Recurse $vst3 $target
    Write-Host "copied to $target"
}

Step "Done"
Write-Host "the profile-guided plugin: $((Resolve-Path $vst3).Path)"
Write-Host "rerun this after pulling new code: the profile is for these exact sources"
