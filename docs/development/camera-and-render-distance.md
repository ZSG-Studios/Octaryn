# Camera and render-distance repair

2026-09-13. First person submits only triangles whose nonzero skin influences
belong to the original left/right arm joints. The original 216-index full player
mesh remains available in third person; the first-person subset is 72 indices.
This restores mesh visibility without replacing the model or shader pipeline.

Third person defaults to the right shoulder. V swaps left/right; F4 switches
first/third person; Q remains descend. The camera sweeps the combined four-metre
backward boom and 0.75-metre horizontal shoulder offset against terrain, including
unknown cells. Obstructions shorten the boom; there is no centered shoulder mode.
Picking uses the displayed camera ray. Reach and outbound edit origins still use
the player's authoritative eye. Server validation rules were not expanded.

The original reference `source/core/render_distance.h` contains seven options:
4, 8, 12, 16, 20, 24, 32. These are restored in the settings owner. The recovery
cap of 4 is removed, and Apply now updates LocalSession, WorldStream and the
renderer during the live session. Default startup remains 4 unless saved settings
or `--render-distance` override it. The active protocol's radius semantics remain:
32 requests 65 by 65 columns. This is not a claim that the original engine's
different `distance + 2` window dimensions were ported. The superseded expanded
4..128 proposal is not the original selector; support beyond 32 remains future work.

Generation now schedules center-first candidates once per snapshot/window revision.
The delivery queue remains bounded to two, with exact query/render payload pairing
and worker-owned retirement. Columns compact after authoritative overrides into
lossless shared pages. Query and renderer copies share that storage. Reads do not
expand it, and mutations detach. The existing GPU halo upload remains explicit.

## Maximum-distance failure and dependency repair

The first radius-32 qualification stalled with 3,678 columns in the last telemetry
sample. CLI thread sampling identified `amdvlk64.dll` on hot thread 18464 of test
process 2092. Disassembly of sampled EXE return address +0x6185f2 identifies
`vkAllocateDescriptorSets`, called from SlangRHI's allocator. It was not a terrain
worker loop or `vkQueueWaitIdle`. Only that owned test client/server were stopped.

The checked-in `tools/build/patches/slang-rhi-descriptor-capacity.patch` adds
per-type descriptor, set, and inline-uniform-binding budgets to pinned SlangRHI.
It selects a pool with enough capacity before calling the driver, restores budgets
on successful free/reset, and propagates allocation failure. The canonical RHI
bootstrap accepts exactly this patch and rejects unrelated tracked modifications.
All rendering still uses Slang shaders and standalone SlangRHI.

Pool accounting follows the distinct limits documented for
[vkAllocateDescriptorSets](https://docs.vulkan.org/refpages/latest/refpages/source/vkAllocateDescriptorSets.html).
The reproduced hang establishes this driver/allocation path; it does not establish
a general failure of all drivers when a pool is full.

## Qualification

- `build/shoulder-distance-final-build.log`: arm membership, 67 shoulder geometry
  checks, real-stream picking/reach, 24,480 terrain parity samples, compact storage,
  4,225-column scheduling and grow/shrink/return invariants pass.
- Ten generated signed-coordinate columns average 73,607 retained bytes, maximum
  112,408, compared with a 1,048,576-byte dense payload previously copied twice.
- `build/shoulder-distance-rhi-build.log`: patched standalone RHI builds.
- `build/shoulder-distance-rhi-client.log`: coherent client/server bundle installs;
  production player rendering passes 4,096 instances per frame over two frames,
  pool growth/reset, exact HDR/depth comparison and zero graphics diagnostics.
- `logs/client/descriptor-allocator-mock.log`: the actual allocator passes 24,576
  mock API allocations across two frames, 24 reused pools, no calls against
  insufficient capacity, free/reset, oversized layouts and failure handling.
- `logs/client/shoulder-distance32-fixed.log`: all 4,225 columns load, 351 fully
  resident frames complete, and the process exits zero. Final GPU mesh has
  108,744,227 quads and estimated resident GPU bytes 1,805,085,344. The three-second
  post-warmup sample averages 22.896 ms (about 44 FPS), 1% low 36.36 FPS. This is
  a short stationary Windows/RX 9070 XT result, not a smooth-travel qualification.
  Streaming still has frame spikes and three playback underruns were observed.
- `logs/client/shoulder-left-validation.log` and corresponding error log: left
  shoulder capture, 81 resident columns, 573 UI checks and no graphics errors.
- `logs/client/shoulder-right-final.log`: final patched-RHI right-shoulder run,
  2,177 resident frames, 573 UI checks and no graphics diagnostics.
- `logs/client/first-person-distance-changes.log`: production Apply changes
  radius 4 -> 8 -> 4, with 81 -> 289 -> 81 actual renderer columns and zero exit.
  All three phases require rendered frames; incomplete runs fail qualification.
- `logs/client/first-person-final.log`: packaged first-person arm capture,
  2,031 resident frames, 573 UI checks and zero graphics diagnostics.
- `logs/client/shoulder-distance-final-stage.json`: 151 installed payload files,
  787 source/code files within the 500-line limit, owner boundaries and active
  Slang/RHI graph verified. No test client/server remains running after qualification.
- Runtime qualification uses isolated validation worlds and domain-level Apply;
  it injects no OS input and leaves production saves and backups untouched.

CLI launch options: `tools/run-client.ps1 -Shoulder left -RenderDistance 8`, or
`-Shoulder right`. Without `-Shoulder`/`-ThirdPerson`, startup is first person.
`--validate-distance-changes` qualifies 4 -> 8 -> 4 through the production Apply
handler and fails if it exits before finishing. Larger distance performance,
remaining rendering stalls, remote networking and other desktops remain separate
qualification work.
