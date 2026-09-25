# Explicit offline content cook. Never invoke automatically during gameplay.
param(
    [string]$Blender = 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe',
    [string]$OutputDirectory = 'build/release-windows/basegame/map-tangent-spatial-candidate',
    [double]$MinimumStartFreeGiB = 12,
    [double]$MinimumFreeGiB = 6,
    [double]$MaximumPrivateGiB = 6,
    [int]$TimeoutSeconds = 1800
)
$ErrorActionPreference = 'Stop'
if ($MinimumStartFreeGiB -lt $MinimumFreeGiB -or $MinimumFreeGiB -le 0 -or
    $MaximumPrivateGiB -le 0 -or $TimeoutSeconds -le 0) { throw 'Invalid cook budget' }
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$candidate = [IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'build/release-windows/basegame')) + [IO.Path]::DirectorySeparatorChar
if (-not $candidate.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Candidate directory must remain inside build/release-windows/basegame'
}
if (Test-Path -LiteralPath $candidate) { throw 'Choose a fresh candidate directory; nothing is overwritten' }
function Get-FreeGiB { (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB }
if ((Get-FreeGiB) -lt $MinimumStartFreeGiB) { throw 'Insufficient free memory for an offline Blender cook' }
$source = Join-Path $repo 'build/dependencies/bistro/Bistro_v5_2'
$converted = Join-Path $repo 'build/dependencies/bistro/converted'
$alpha = Join-Path $repo 'build/release-windows/basegame/map-import/alpha_modes.json'
$importer = Join-Path $PSScriptRoot 'ImportFbxMap.py'
$arguments = @('-b', '--factory-startup', '-t', '1', '--python-exit-code', '1', '--python',
    $importer, '--', '--fbx', (Join-Path $source 'BistroExterior.fbx'),
    (Join-Path $source 'BistroInterior_Wine.fbx'), '--textures', $converted,
    '--alpha-modes', $alpha, '--cell-size', '16', '--output', (Join-Path $candidate 'main.glb'),
    '--report', (Join-Path $candidate 'report.json'))
New-Item -ItemType Directory -Path $candidate | Out-Null
$quoted = ($arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }) -join ' '
$process = Start-Process -FilePath $Blender -ArgumentList $quoted -WorkingDirectory $repo -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $candidate 'stdout.log') -RedirectStandardError (Join-Path $candidate 'stderr.log')
$watch = [Diagnostics.Stopwatch]::StartNew()
try {
    $process.PriorityClass = 'BelowNormal'
    while (-not $process.WaitForExit(2000)) {
        $process.Refresh()
        if ($process.PrivateMemorySize64 -gt $MaximumPrivateGiB * 1GB -or
            (Get-FreeGiB) -lt $MinimumFreeGiB -or $watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
            throw 'Offline cook memory/time budget exceeded; candidate remains unvalidated'
        }
    }
    if ($process.ExitCode -ne 0) { throw "Blender cook failed: $($process.ExitCode)" }
    Write-Output "Candidate generated at $candidate; validate before staging. Active map was not changed."
} finally {
    if (-not $process.HasExited) {
        # Only the process started above and its children may be stopped.
        & taskkill.exe /PID $process.Id /T /F | Out-Null
    }
    $process.Dispose()
}
