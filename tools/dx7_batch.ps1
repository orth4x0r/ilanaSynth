# Converts the DX7 voices behind the FM presets, renders them dry and
# compares each with its msfa reference. Writes build/dx7-voicing.txt (the
# new lines) and build/dx7-report.txt.
#   powershell -File tools/dx7_batch.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$roms = "build\reference\dx7\ddxx7\public\assets\patches"

# preset name, bank, voice, home note (as ilanaPresetRender plays it)
$map = @(
    @("FM E-Piano",    "rom1a", 11, 60),
    @("FM Donk",       "rom1a", 15, 36),
    @("FM Brass Lead", "rom4a",  7, 60),
    @("FM Pluck",      "rom1a", 12, 60),
    @("Kalimba",       "rom1a", 22, 60),
    @("FM Kalimba",    "rom1b",  7, 60),
    @("Operator Bell", "rom1a", 26, 60),
    @("Metallic Hit",  "rom2a", 26, 60),
    @("FM Sequence",   "rom3a", 16, 60)
)

Remove-Item build\dx7-voicing.txt, build\dx7-report.txt -ErrorAction SilentlyContinue
foreach ($m in $map) {
    $name, $bank, $voice, $note = $m
    cmd /c "python tools\dx7_import.py $roms\$bank.syx $voice ""$name"" --hold 2 --total 3.5 --note $note 2>> build\dx7-report.txt >> build\dx7-voicing.txt"
}
python tools\bake_voicing.py --extract build\voicing-base.txt | Out-Null
python tools\voicing_merge.py build\voicing-base.txt build\dx7-voicing.txt -o build\voicing-dx7-dry.txt --dry
python tools\voicing_merge.py build\voicing-base.txt build\dx7-voicing.txt -o build\voicing-dx7.txt

$env:ILANA_PRESET_VOICING = "$root\build\voicing-dx7-dry.txt"
# Preset indices come from a full render's index (made once); then only
# the nine are rendered.
if (-not (Test-Path build\dx7render-ours\index.csv)) {
    & .\build\ilanaPresetRender_artefacts\Release\ilanaPresetRender.exe build\dx7render-ours | Out-Null
}
$index = Import-Csv build\dx7render-ours\index.csv
foreach ($m in $map) {
    $row = $index | Where-Object { $_.name -eq $m[0] }
    & .\build\ilanaPresetRender_artefacts\Release\ilanaPresetRender.exe build\dx7render-ours $row.index 1 | Out-Null
}
$env:ILANA_PRESET_VOICING = ""
foreach ($m in $map) {
    $name, $bank, $voice, $note = $m
    $row = $index | Where-Object { $_.name -eq $name }
    $tag = (python -c "import sys; sys.path.insert(0, 'tools'); import dx7_import as d; from pathlib import Path; v = d.unpack(Path(r'$roms\$bank.syx').read_bytes(), $voice); print(''.join(c if c.isalnum() else '_' for c in v['name'].strip()).lower())")
    $f0 = 440.0 * [Math]::Pow(2, ($note - 69) / 12.0)
    "== $name ($bank $voice, $tag) ==" | Add-Content build\dx7-report.txt
    python tools\dx7_compare.py "build\reference\dx7\${tag}_$note.wav" "build\dx7render-ours\$($row.index)\note.wav" --f0 $f0 | Add-Content build\dx7-report.txt
}
