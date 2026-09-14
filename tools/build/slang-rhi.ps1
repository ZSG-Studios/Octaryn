[CmdletBinding()]
param(
    [ValidateSet('x64', 'arm64')][string]$Architecture = 'x64',
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [string]$SlangSdkRoot = '',
    [int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$dependencyRoot = Join-Path $repoRoot 'build/dependencies'
$sourceRoot = Join-Path $dependencyRoot 'slang-rhi'
$buildRoot = Join-Path $dependencyRoot "slang-rhi-windows-$Architecture-$Configuration"
$commit = 'e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc'
if (-not $SlangSdkRoot) { $SlangSdkRoot = Join-Path $dependencyRoot 'slang-2026.17.1' }
$SlangSdkRoot = [IO.Path]::GetFullPath($SlangSdkRoot).Replace('\', '/')
foreach ($relative in @('include/slang.h', 'bin/slangc.exe', 'bin/slang-compiler.dll', 'lib/slang-compiler.lib')) {
    if (-not (Test-Path -LiteralPath (Join-Path $SlangSdkRoot $relative))) { throw "Incomplete Slang SDK: $relative" }
}
if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot '.git'))) {
    & git init $sourceRoot
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize slang-rhi checkout.' }
    & git -C $sourceRoot fetch --depth 1 https://github.com/shader-slang/slang-rhi.git $commit
    if ($LASTEXITCODE -ne 0) { throw 'Could not fetch the pinned slang-rhi source.' }
    & git -C $sourceRoot checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Could not check out the pinned slang-rhi source.' }
}
$actualCommit = & git -C $sourceRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $commit) { throw "Expected slang-rhi commit $commit; found $actualCommit. Existing checkout was not changed." }
& (Join-Path $PSScriptRoot 'apply-slang-rhi-patch.ps1') -SourceRoot $sourceRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Visual Studio C++ build tools were not found.' }
& (Join-Path $vsRoot 'Common7/Tools/Launch-VsDevShell.ps1') -Arch $Architecture -HostArch amd64 -SkipAutomaticLocation
if ($env:VSCMD_ARG_TGT_ARCH -ne $Architecture) { throw 'Visual Studio target environment was not initialized.' }
$toolDirs = @((Join-Path $dependencyRoot 'tools/cmake/bin'), (Join-Path $dependencyRoot 'tools/ninja'), (Join-Path $env:ProgramFiles 'LLVM/bin'))
$env:PATH = ($toolDirs -join ';') + ';' + $env:PATH
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$ninja = (Get-Command ninja -ErrorAction Stop).Source
$clang = (Get-Command clang-cl -ErrorAction Stop).Source
$options = @(
    '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_C_COMPILER=$clang", "-DCMAKE_CXX_COMPILER=$clang",
    "-DCMAKE_PROJECT_INCLUDE=$repoRoot/cmake/Dependencies/SlangRhiCompiler.cmake",
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL',
    '-DSLANG_RHI_BUILD_SHARED=OFF', '-DSLANG_RHI_BUILD_TESTS=OFF',
    '-DSLANG_RHI_BUILD_TESTS_WITH_GLFW=OFF', '-DSLANG_RHI_BUILD_EXAMPLES=OFF',
    '-DSLANG_RHI_INSTALL=OFF', '-DSLANG_RHI_FETCH_SLANG=OFF', '-DSLANG_RHI_FETCH_DXC=ON',
    "-DSLANG_RHI_SLANG_INCLUDE_DIR=$SlangSdkRoot/include", "-DSLANG_RHI_SLANG_BINARY_DIR=$SlangSdkRoot",
    '-DSLANG_RHI_ENABLE_VULKAN=ON', '-DSLANG_RHI_ENABLE_D3D12=ON'
)
if ($Architecture -eq 'arm64') { $options += '-DCMAKE_C_COMPILER_TARGET=aarch64-pc-windows-msvc'; $options += '-DCMAKE_CXX_COMPILER_TARGET=aarch64-pc-windows-msvc' }
foreach ($backend in @('CPU', 'D3D11', 'AGILITY_SDK', 'NVAPI', 'METAL', 'CUDA', 'OPTIX', 'WGPU', 'AFTERMATH')) {
    $options += "-DSLANG_RHI_ENABLE_$backend=OFF"
}
& $cmake -S $sourceRoot -B $buildRoot @options
if ($LASTEXITCODE -ne 0) { throw 'Standalone slang-rhi configure failed.' }
& $cmake --build $buildRoot --target slang-rhi --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'Standalone slang-rhi build failed.' }
$sdkCmakePath = $SlangSdkRoot.Replace('\', '/')
@"
set(OCTARYN_SLANG_RHI_BUILT_COMMIT "$commit")
set(OCTARYN_SLANG_RHI_BUILT_SDK_ROOT "$sdkCmakePath")
set(OCTARYN_SLANG_RHI_BUILT_ARCH "$Architecture")
set(OCTARYN_SLANG_RHI_BUILT_CONFIG "$Configuration")
"@ | Set-Content -LiteralPath (Join-Path $buildRoot 'octaryn-dependency.cmake') -Encoding utf8
Write-Host "Standalone static DX12/Vulkan slang-rhi ready: $buildRoot"
