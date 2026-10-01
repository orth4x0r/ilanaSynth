# Self-hosted CI runner

The account has no GitHub-hosted Actions minutes left, so the gate runs on the
user's own Windows PC through a self-hosted runner. The repository is private, so
only people with write access can start a run.

## Set up (once, on the Windows PC)
1. github.com/orth4x0r/ilanaSynth > Settings > Actions > Runners > **New
   self-hosted runner** > Windows. Copy the `--token` value (valid for an hour).
2. In a PowerShell window in the checkout:
   `powershell -ExecutionPolicy Bypass -File tools\setup-runner.ps1 -Token <token> -AtLogon`
   It downloads the runner to `%USERPROFILE%\actions-runner-ilanasynth`, registers
   it with the label `windows`, and (with `-AtLogon`) adds a Task Scheduler task,
   "ilanaSynth runner", that starts it at each logon at below-normal priority.
   Without `-AtLogon`, start it by hand with `run.cmd` in that folder.
   A logon task, not a Windows service: the UI tests open windows, which a
   service cannot.

## Run the gate
- Actions > **Self-hosted gate** > Run workflow: pick `windows`, optionally
  *Skip pluginval*, and the build parallelism (default 1: the PC runs out of
  memory with more).
- Agents dispatch the same workflow through the GitHub API (`self-hosted.yml`,
  inputs `os`, `quick`, `jobs`) and read the result and the uploaded
  `gate-windows` artifact (unit test log, fingerprints, pluginval log).
- It runs `tools/verify.ps1` (the Windows twin of `tools/verify.sh`) in the
  runner's own checkout (`_work`), separate from the user's build folder. The
  build tree is kept between runs; the first run builds JUCE and takes longest.
- A run queued while the PC is off waits (up to a day) for the runner.
- Linux or macOS machines can join the same way with the label `linux` or
  `macos`; their job runs `tools/verify.sh` (`ILANA_JOBS` sets the parallelism).

## Fingerprints on Windows
They differ from Linux in the last bits, so `verify.ps1` compares only against
`tests/fingerprints-windows.csv`, which does not exist yet. To make the baseline,
copy a clean run's `build\fingerprints.csv` there and commit it.

## Stop or remove
- Pause: Task Scheduler > "ilanaSynth runner" > End (or Disable).
- Remove: in the runner folder, `.\config.cmd remove --token <token>` (a removal
  token from the same Runners page), then delete the folder and the task
  (`Unregister-ScheduledTask "ilanaSynth runner"`).
