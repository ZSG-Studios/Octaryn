# Current-frame virtual geometry bin dispatch

Recorded 2026-10-01 on Windows, AMD Radeon RX 9070 XT, DX12 with required API
core validation. This is a correction to the existing Slang-RHI renderer, not
qualification of the planned NVRHI renderer.

## Failure and change

The Novac interactive session reported meshes disappearing and returning during
camera movement after all 85 geometry pages were resident. Pending pages,
selection error and overflow were zero. Static captures had been identical;
that evidence did not qualify motion.

`OcclusionGpu::begin` reset and classified the current selected cluster list,
then the early hybrid raster passes consumed the bank's indirect dispatch
arguments. Those arguments were only finalized during `retest`, after early
visibility. A reused bank therefore supplied older counts alongside the current
cluster list. If the current hardware count grew beyond the old dispatch's
32-cluster amplification groups, entire clusters were omitted. Growing software
counts were similarly under-dispatched. Static views could converge while
moving views repeatedly lost geometry. Initial bank arguments also lacked this
required publication before first use.

The correction records the existing `bin_finalize_main` immediately after early
classification in `begin`, using the existing explicit barriers. Retesting still
finalizes the late phase afterward. Rasterization, culling, depth, material
rules, screen error, pool size and watchdog limits are unchanged.

## Executed GPU regression

The focused probe completes a legitimate hardware-one/software-zero warmup
frame through the normal API. It then changes the same bank's counts and copies
indirect arguments immediately after `begin`, before depth/retest/finish could
repair them. The classification list heads are checked independently against
the authored fixture counts.

| Executed source | Current hardware/software | Early mesh groups | Early software groups | Result |
| --- | --- | --- | --- | --- |
| Before fix | 33 / 5 | 1, expected 2 | 0, expected 5 | Intended failure, exit 1 |
| After fix | Growing, shrinking and zero counts | Current counts | Current counts | Four cases pass, exit 0 |

Evidence is saved in sibling `OpenFNV/logs/`:

- `occlusion-bin-probe-before-v2.log` and `.json`: intended regression failure,
  0.934 seconds; resource retirement pending zero.
- `occlusion-bin-probe-after-v1.log` and `.json`: pass, 0.680 seconds; required
  GPU core validation and resource retirement pending zero.
- `occlusion-bin-probe-before.log` and `.json`: an earlier fixture admission
  failure; it did not reproduce the rendering bug. The fixture was corrected to
  seed a previous frame through normal API calls instead of uploading to a
  buffer that has no copy-destination usage.

Build and execute the focused entry from the engine root, with the existing
client bundle on the child process DLL search path:

```powershell
python tools/build/windows.py --action build --preset release-windows --target octaryn_virtual_geometry_gpu_probe --jobs 8
build/release-windows/tools/virtual-geometry/octaryn_virtual_geometry_gpu_probe.exe dx12 --occlusion-bins
```

The recorded runs used hidden process creation and a 60-second external deadline;
the fixture's GPU fence deadline is 30 seconds. No CTest or broad GPU suite was
run for this regression. The focused build passes. Shader implicit-conversion
warnings remain separate from API validation errors.

## Motion evidence and limits

`OCTARYN_CLIENT_VIRTUAL_GEOMETRY_FRAME_DIAGNOSTICS=1` adds current camera/page
state and completed selection feedback tagged with its original render frame.
It uses existing fenced readbacks; it adds no GPU buffers, waits or quality
changes. Feedback slot/generation identity is also asserted in the existing
selection parity fixture for its next normal run.

Two attempts to capture the pre-fix full rotating scene failed the unchanged
startup frame-time watchdog; their receipts are retained under
`OpenFNV/logs/source-cell-loop-before` and `source-cell-loop-before-warm`. They
do not supply a passing visual baseline, and their startup stalls have not been
separately attributed. The focused GPU before/after failure is the direct proof
of the dispatch error.

The canonical fixed bundle builds successfully. The hidden, capped post-fix
interior orbit run completed 650 frames, 16 captures and clean retirement under
the unchanged watchdog (`OpenFNV/logs/source-cell-loop-after`). Eight camera
poses captured 128 frames apart have identical recorded eye/yaw/pitch and
identical BMP bytes: zero differing pixels in every pair. All eight unique
images were inspected; no omitted cluster holes were observed in their room
walls, ceiling, door, furniture, shelf or sink geometry.

Across 319 warmed render frames and 317 completed feedback samples, all 85 pages
remain resident. Selected counts range from 87 to 298; pending pages, requested
pages, selection error, overflow and missing roots remain zero. Original frame,
feedback slot and generation are retained in
`OpenFNV/logs/source-cell-loop-after/renderer-motion-comparison.json`. The final
two render frames have no completed feedback sample in this receipt.

This qualifies the recorded warmed DX12 orbit and the corrected dispatch-count
contract. It is not exhaustive arbitrary-camera coverage or a performance
qualification. The source import still has independent material, lighting and
gameplay limitations. Vulkan, other GPU vendors and full-cell parity remain
unqualified.
