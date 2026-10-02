# Renders the nine FM presets before (built-in voicing) and after
# (build/voicing-dx7.txt), then sets each new line's master+= so the new
# preset is as loud as the old one. Each render has a time limit.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$exe = "$root\build\ilanaPresetRender_artefacts\Release\ilanaPresetRender.exe"
$names = "FM E-Piano", "FM Donk", "FM Brass Lead", "FM Pluck", "Kalimba", "FM Kalimba", "Operator Bell", "Metallic Hit", "FM Sequence"
$index = Import-Csv build\dx7-index-backup.csv

function Render($dir, $i, $voicing) {
    $env:ILANA_PRESET_VOICING = $voicing
    $p = Start-Process -FilePath $exe -ArgumentList "$dir", "$i", "1" -NoNewWindow -PassThru
    if (-not $p.WaitForExit(240000)) { $p.Kill(); Write-Host "TIMEOUT rendering $i into $dir" }
    $env:ILANA_PRESET_VOICING = ""
}

foreach ($dir in "build\dx7-old", "build\dx7-new") { Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue }
foreach ($n in $names) {
    $i = ($index | Where-Object { $_.name -eq $n }).index
    $t = Measure-Command { Render "$root\build\dx7-old" $i "" }
    $u = Measure-Command { Render "$root\build\dx7-new" $i "$root\build\voicing-dx7.txt" }
    Write-Host ("{0}: old {1:n0} s, new {2:n0} s" -f $n, $t.TotalSeconds, $u.TotalSeconds)
}
Copy-Item build\dx7-index-backup.csv build\dx7-old\index.csv -Force
Copy-Item build\dx7-index-backup.csv build\dx7-new\index.csv -Force
python tools\voicing_level_match.py build\dx7-old build\dx7-new build\dx7-voicing.txt
python tools\voicing_merge.py build\voicing-base.txt build\dx7-voicing.txt -o build\voicing-dx7.txt
