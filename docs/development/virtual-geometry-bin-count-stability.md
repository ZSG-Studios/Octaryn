# Current-frame virtual geometry bin dispatch

Recorded 2026-10-01 on Windows, AMD Radeon RX 9070 XT, DX12 with required API
core validation. This is a correction to the existing Slang-RHI renderer, not
qualification of the planned NVRHI renderer.

## Failure and change

Meshes were reported disappearing and returning during camera movement with
all geometry pages resident and zero pending pages, selection error and
overflow. Static captures had been identical; that evidence did not qualify motion.

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

The fixture seeds a previous frame through normal API calls rather than
uploading to a buffer that has no copy-destination usage.

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

Full-scene motion captures before and after the fix are not yet qualified in
this repo. The focused GPU before/after failure is the direct proof of the
dispatch error.

This qualifies the corrected dispatch-count contract only. It is not
arbitrary-camera coverage or a performance qualification. Vulkan and other
GPU vendors remain unqualified.
