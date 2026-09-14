param(
    [string]$WorldDirectory,
    [switch]$ThirdPerson,
    [ValidateSet('left', 'right')][string]$Shoulder,
    [ValidateSet(4, 8, 12, 16, 20, 24, 32)][int]$RenderDistance
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$client = Join-Path $repo 'build/release-windows/client/bundle/Octaryn.Client.exe'
if (!(Test-Path -LiteralPath $client -PathType Leaf)) {
    throw 'Build the client first: tools/build/windows.ps1 -Action build -Target octaryn_client_bundle'
}
# The single natural generator includes vegetation. Keep older saves preserved.
if (!$WorldDirectory) { $WorldDirectory = Join-Path $repo 'saves/open-world-v3' }
$world = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($WorldDirectory)
$options = @{
    FilePath = $client
    WorkingDirectory = Split-Path -Parent $client
    PassThru = $true
}
$clientArguments = @()
if ($ThirdPerson) { $clientArguments += '--third-person' }
if ($Shoulder) { $clientArguments += @('--shoulder', $Shoulder) }
if ($RenderDistance) { $clientArguments += @('--render-distance', [string]$RenderDistance) }
if ($clientArguments.Count) { $options.ArgumentList = $clientArguments }
$previousWorld = $env:OCTARYN_CLIENT_WORLD_PATH
try {
    $env:OCTARYN_CLIENT_WORLD_PATH = $world
    $process = Start-Process @options
} finally {
    $env:OCTARYN_CLIENT_WORLD_PATH = $previousWorld
}
Write-Output "Octaryn started: process=$($process.Id) world=$world"
