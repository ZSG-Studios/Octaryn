[CmdletBinding()]
param(
    [ValidateSet('configure', 'build', 'run-client')][string]$Action = 'build',
    [ValidateSet('debug-windows', 'release-windows')][string]$Preset = 'release-windows',
    [ValidateSet('x64', 'arm64')][string]$Architecture = 'x64',
    [string[]]$Target = @('octaryn_all'),
    [int]$Jobs = 8,
    [string[]]$ConfigureArgument = @(),
    [string[]]$ClientArgument = @()
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$presetRoot = $Preset
if ($Architecture -ne 'x64') { $presetRoot += "-$Architecture" }
$binaryDir = Join-Path $repoRoot "build/$presetRoot/cmake"
if ($Action -eq 'run-client') {
    $bundle = Join-Path $repoRoot "build/$presetRoot/client/bundle"
    $client = Join-Path $bundle 'Octaryn.Client.exe'
    if (-not (Test-Path -LiteralPath $client)) { throw "Build the client bundle first: $client" }
    Push-Location $bundle
    try {
        & $client @ClientArgument
        if ($LASTEXITCODE -ne 0) { throw "Client exited with code $LASTEXITCODE." }
    } finally { Pop-Location }
    return
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ build tools.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Visual Studio C++ build tools were not found.' }
& (Join-Path $vsRoot 'Common7/Tools/Launch-VsDevShell.ps1') -Arch $Architecture -HostArch amd64 -SkipAutomaticLocation
if ($env:VSCMD_ARG_TGT_ARCH -ne $Architecture) { throw 'Visual Studio target environment was not initialized.' }

$toolRoot = Join-Path $repoRoot 'build/dependencies/tools'
$toolDirs = @(
    (Join-Path $toolRoot 'cmake/bin'),
    (Join-Path $toolRoot 'ninja'),
    (Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin'),
    (Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja'),
    (Join-Path $env:ProgramFiles 'LLVM/bin')
)
$env:PATH = ($toolDirs -join ';') + ';' + $env:PATH
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$null = Get-Command ninja -ErrorAction Stop
$env:OCTARYN_TARGET_ARCH = $Architecture
Push-Location $repoRoot
try {
    if ($Action -eq 'configure') {
        & $cmake --preset $Preset -B $binaryDir "-DOCTARYN_TARGET_ARCH=$Architecture" @ConfigureArgument
    } else {
        & $cmake --build $binaryDir --target @Target --parallel $Jobs
    }
    if ($LASTEXITCODE -ne 0) { throw "CMake $Action failed with exit code $LASTEXITCODE." }
} finally {
    Pop-Location
}
