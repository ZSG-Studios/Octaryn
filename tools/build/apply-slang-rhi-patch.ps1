param([Parameter(Mandatory=$true)][string]$SourceRoot)
$ErrorActionPreference = 'Stop'
$patches=@('slang-rhi-descriptor-capacity.patch','slang-rhi-multi-draw-capabilities.patch','slang-rhi-d3d12-sampler-cache.patch','slang-rhi-d3d12-draw-capabilities.patch')
$allowed=@()
foreach($name in $patches) {
    $patch=Join-Path $PSScriptRoot "patches/$name"
    $expected=[IO.File]::ReadAllText($patch).Replace("`r`n","`n").TrimEnd("`n")
    $paths=@([regex]::Matches($expected,'(?m)^diff --git a/(\S+) b/\S+$') | ForEach-Object {$_.Groups[1].Value})
    if(!$paths.Count){throw "No file list in pinned patch $name"}
    $allowed+=$paths
    $actual=(& git -C $SourceRoot diff --binary --no-ext-diff HEAD -- @paths) -join "`n"
    if($LASTEXITCODE -ne 0){throw 'Cannot inspect pinned slang-rhi changes.'}
    if(!$actual) {
        & git -C $SourceRoot apply --check $patch
        if($LASTEXITCODE -ne 0){throw "Patch does not match pinned slang-rhi: $name"}
        & git -C $SourceRoot apply $patch
        if($LASTEXITCODE -ne 0){throw "Could not apply pinned patch: $name"}
        $actual=(& git -C $SourceRoot diff --binary --no-ext-diff HEAD -- @paths) -join "`n"
        if($LASTEXITCODE -ne 0){throw 'Cannot inspect patched slang-rhi changes.'}
    }
    if($actual.Replace("`r`n","`n").TrimEnd("`n") -cne $expected) {
        throw "Pinned slang-rhi changes differ from exact checked-in patch: $name"
    }
}
$changed=@(& git -C $SourceRoot diff --name-only HEAD)
if($LASTEXITCODE -ne 0){throw 'Cannot inspect pinned slang-rhi file list.'}
foreach($path in $changed){if($path -notin $allowed){throw "Unapproved pinned slang-rhi change: $path"}}
