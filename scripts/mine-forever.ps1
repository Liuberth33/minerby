#requires -Version 5
# Keep minerby running: relaunch it whenever it exits. Ctrl+C to stop.
# Usage: ./scripts/mine-forever.ps1 [path\to\config.json]
$ErrorActionPreference = 'Continue'

$root   = Split-Path -Parent $PSScriptRoot
$exe    = Join-Path $root 'build\minerby.exe'
$config = if ($args.Count -ge 1) { $args[0] } else { Join-Path $root 'config.json' }
$log    = Join-Path $root 'minerby-run.log'

if (-not (Test-Path $exe))    { throw "not built: $exe (run scripts\build.ps1)" }
if (-not (Test-Path $config)) { throw "no config: $config" }

Write-Host "minerby supervisor - config: $config - log: $log"
while ($true) {
  $ts = Get-Date -Format o
  "[$ts] launching minerby" | Tee-Object -FilePath $log -Append
  & $exe run --config $config 2>&1 | Tee-Object -FilePath $log -Append
  $code = $LASTEXITCODE
  "[$(Get-Date -Format o)] minerby exited ($code); restarting in 15s" | Tee-Object -FilePath $log -Append
  Start-Sleep -Seconds 15
}
