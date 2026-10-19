param([string]$TestClient = '')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'prepare-dependencies.ps1')
& git -C $repoRoot submodule update --init --recursive
if ($LASTEXITCODE -ne 0) { throw 'Submodule setup failed' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual Studio C++ x86 tools are required' }
$msbuild = Join-Path $vsPath 'MSBuild/Current/Bin/MSBuild.exe'
& $msbuild (Join-Path $repoRoot 'mapleroot/ijl15/ijl15.vcxproj') /p:Configuration=Release /p:Platform=Win32 /p:MapleRootIntegration=true /p:PostBuildEventUseInBuild=false /m /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw 'MapleRoot companion build failed' }
$buildDir = Join-Path $repoRoot 'build-baseline'
$configure = @('-S', $repoRoot, '-B', $buildDir, '-G', 'Visual Studio 17 2022', '-A', 'Win32', '-DMAPLEROOT_INTEGRATION=ON', '-DMAPLEROOT_SHUTDOWN_SMOKE_TEST=OFF', '-DMAPLEROOT_CASHSHOP_WINDOW=ON', '-DMAPLEROOT_WEATHER=ON', '-DMAPLEROOT_MOVEMENT_KEYS=ON', '-DMAPLEROOT_MOUNT_INFO=ON')
if ($TestClient) { $configure += "-DMOVEMENTKEYS_TEST_CLIENT=$TestClient" }
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'Client configuration failed' }
& cmake --build $buildDir --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Client build failed' }
& ctest --test-dir $buildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed' }
Write-Host "Built Kaentake files: $buildDir/Release; companion: $repoRoot/mapleroot/build/Integrated"
