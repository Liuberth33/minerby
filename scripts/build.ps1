#requires -Version 5
# Configure + build minerby with the MSVC toolchain, without needing an
# already-open Developer prompt. Usage: ./scripts/build.ps1 [buildDir]
$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent $PSScriptRoot
$buildDir = if ($args.Count -ge 1) { $args[0] } else { 'build' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install Visual Studio Build Tools with the C++ workload." }

$vsPath = & $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
if (-not $vsPath) { throw "No MSVC C++ toolset found via vswhere." }

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
$cmake  = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = 'cmake' }

Write-Host "MSVC:  $vsPath"
Write-Host "CMake: $cmake"
Write-Host "Out:   $root\$buildDir"

$cfg = "`"$vcvars`" && `"$cmake`" -S `"$root`" -B `"$root\$buildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release && `"$cmake`" --build `"$root\$buildDir`""
& cmd /c $cfg
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }

Write-Host "`nDone. Binary: $root\$buildDir\minerby.exe"
