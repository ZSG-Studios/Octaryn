param(
    [Parameter(Mandatory = $true)][string]$NativeDirectory,
    [string]$SharedNativeDirectory,
    [ValidateSet('release-windows')][string]$Preset = 'release-windows',
    [switch]$NoBuild
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$native = (Resolve-Path $NativeDirectory).Path
if (!$SharedNativeDirectory) { $SharedNativeDirectory = $native }
$shared = (Resolve-Path $SharedNativeDirectory).Path
$project = Join-Path $root 'tools/validation/Octaryn.ServerWorldBlocksProbe/Octaryn.ServerWorldBlocksProbe.csproj'
$qualification = Join-Path $root "build/$Preset/tools/validation/block-prediction-qualification"
$assembly = Join-Path $qualification 'managed/Octaryn.ServerWorldBlocksProbe.dll'
$log = Join-Path $root 'logs/tools/block-receipt-production-qualification.log'
$saved = @{}
$variables = @('HOST','WORLD_PERSISTENCE','PLAYER_SIMULATION','AUTHORITY_TICK',
    'BLOCK_STORE','TERRAIN_GENERATION','WORLD_ITEMS','WORLD_TIME')
Push-Location $root
try {
    foreach ($name in $variables) {
        $key = "OCTARYN_SERVER_${name}_LIBRARY"
        $saved[$key] = [Environment]::GetEnvironmentVariable($key)
        $library = Join-Path $native "octaryn_server_$($name.ToLowerInvariant()).dll"
        if (!(Test-Path $library)) { throw "Missing native production library: $library" }
        [Environment]::SetEnvironmentVariable($key, $library)
    }
    foreach ($key in @('PATH', 'OctarynBuildPresetName', 'OCTARYN_SERVER_WORLD_BLOCKS_PROBE_DIR')) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key)
    }
    $env:PATH = "$native;$shared;$env:PATH"
    $env:OctarynBuildPresetName = $Preset
    $env:OCTARYN_SERVER_WORLD_BLOCKS_PROBE_DIR = Join-Path $qualification 'worlds'
    if (!$NoBuild) {
        & dotnet build $project "-p:OctarynBuildPresetName=$Preset" "-p:OutputPath=$qualification/managed/" `
            "-p:OctarynIntermediateRoot=$qualification/intermediate" --verbosity minimal
        if ($LASTEXITCODE -ne 0) { throw 'Qualification owner build failed.' }
    }
    New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
    & dotnet $assembly --block-receipts *> $log
    if ($LASTEXITCODE -ne 0) {
        Get-Content $log -Tail 40
        throw "Block receipt production qualification failed ($LASTEXITCODE)."
    }
    Get-Content $log -Tail 1
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key]) }
    Pop-Location
}
