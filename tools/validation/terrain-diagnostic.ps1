[CmdletBinding()]
param([string]$OutputDirectory = 'logs/server/terrain-diagnostic')
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Visual Studio C++ build tools were not found.' }
& (Join-Path $vsRoot 'Common7/Tools/Launch-VsDevShell.ps1') -Arch x64 -HostArch amd64 -SkipAutomaticLocation
$compiler = Join-Path $env:ProgramFiles 'LLVM/bin/clang-cl.exe'
$binaryDirectory = Join-Path $repoRoot 'build/release-windows/tools/terrain-validation'
New-Item -ItemType Directory -Force -Path $binaryDirectory | Out-Null
$binary = Join-Path $binaryDirectory 'octaryn_terrain_diagnostic.exe'
$object = Join-Path $binaryDirectory 'TerrainDiagnostic.obj'
$source = Join-Path $repoRoot 'tools/Source/ServerTerrainGenerationProbe/TerrainDiagnostic.cpp'
$include = Join-Path $repoRoot 'octaryn-basegame/Source/Gameplay/Terrain'
& $compiler /nologo /std:c++latest /EHsc /O2 /MD /W4 /WX "/I$include" "/Fo$object" "/Fe$binary" $source
if ($LASTEXITCODE -ne 0) { throw 'Terrain diagnostic compilation failed.' }
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory = Join-Path $repoRoot $OutputDirectory }
& $binary $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'Terrain diagnostic failed.' }
& python (Join-Path $PSScriptRoot 'terrain-diagnostic-images.py') $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'Terrain diagnostic PNG encoding failed.' }
Write-Output "Terrain diagnostic artifacts: $OutputDirectory"
