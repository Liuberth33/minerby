#requires -Version 5
# Register a per-user Scheduled Task that runs mine-forever.ps1 at logon and
# keeps restarting it. No admin needed. Remove with:
#   Unregister-ScheduledTask -TaskName minerby -Confirm:$false
$ErrorActionPreference = 'Stop'

$root   = Split-Path -Parent $PSScriptRoot
$script = Join-Path $PSScriptRoot 'mine-forever.ps1'

$action = New-ScheduledTaskAction -Execute 'powershell.exe' `
  -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$script`""
$trigger = New-ScheduledTaskTrigger -AtLogOn
$settings = New-ScheduledTaskSettingsSet `
  -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
  -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1) `
  -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew

Register-ScheduledTask -TaskName 'minerby' -Action $action -Trigger $trigger `
  -Settings $settings -Description 'minerby CPU miner (auto-restart)' -Force

Write-Host "Installed scheduled task 'minerby'. It starts at your next logon."
Write-Host "Start now:  Start-ScheduledTask -TaskName minerby"
Write-Host "Stop:       Stop-ScheduledTask  -TaskName minerby"
Write-Host "Remove:     Unregister-ScheduledTask -TaskName minerby -Confirm:`$false"
