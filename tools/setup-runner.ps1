# Sets up this Windows PC as a GitHub Actions runner for ilanaSynth, so the
# gate (.github/workflows/self-hosted.yml) runs here instead of on GitHub's
# paid runners. Run once from PowerShell (not as administrator):
#
#   powershell -ExecutionPolicy Bypass -File tools\setup-runner.ps1 -Token <token>
#   ... -AtLogon      also start the runner (at low priority) each time you log on
#
# The token comes from github.com/orth4x0r/ilanaSynth > Settings > Actions >
# Runners > New self-hosted runner (the --token value on that page; it
# expires after an hour). Steps and how to stop or remove the runner:
# docs/SELF-HOSTED-RUNNER.md.
param(
    [Parameter(Mandatory = $true)] [string] $Token,
    [string] $Repo = "https://github.com/orth4x0r/ilanaSynth",
    [string] $Dir = (Join-Path $env:USERPROFILE "actions-runner-ilanasynth"),
    [string] $Name = $env:COMPUTERNAME,
    [switch] $AtLogon
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $Dir | Out-Null
Set-Location $Dir

if (-not (Test-Path config.cmd)) {
    $release = Invoke-RestMethod https://api.github.com/repos/actions/runner/releases/latest
    $asset = $release.assets | Where-Object { $_.name -like "actions-runner-win-x64-*.zip" } | Select-Object -First 1
    Write-Host "Downloading $($asset.name)"
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile runner.zip
    Expand-Archive -Force runner.zip .
    Remove-Item runner.zip
}

# Labels: self-hosted, Windows, X64 come automatically; "windows" is the one
# the workflow asks for.
.\config.cmd --unattended --url $Repo --token $Token --name $Name --labels windows --work _work --replace
if ($LASTEXITCODE -ne 0) { throw "runner configuration failed" }

if ($AtLogon) {
    # A logon task rather than a Windows service: the UI tests open windows,
    # which a service (session 0, no desktop) cannot.
    $action = New-ScheduledTaskAction -Execute (Join-Path $Dir "run.cmd") -WorkingDirectory $Dir
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -ExecutionTimeLimit 0 -Priority 7
    Register-ScheduledTask -TaskName "ilanaSynth runner" -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
    Start-ScheduledTask -TaskName "ilanaSynth runner"
    Write-Host "The runner starts at each logon (Task Scheduler: 'ilanaSynth runner') and is running now."
} else {
    Write-Host "Start the runner with: $Dir\run.cmd (Ctrl+C stops it)."
}
