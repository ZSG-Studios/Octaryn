# Scene preview camera

The executable uses the current Slang-RHI renderer. This work does not qualify
the planned NVRHI migration.

## Preview camera

The authoritative player camera adds its body/eye offset to the map manifest
spawn, so an imported interior can place the eye above an authored ceiling.
Single-sided ceilings then correctly cull their backfaces and the room appears
roofless from outside.

An explicit `OCTARYN_CLIENT_SCENE_PREVIEW_CAMERA=x,y,z,yaw,pitch` requests an
authoring camera independently of the server player pose. It suppresses player
movement and jump inputs and changes only the rendered camera. It does not
adapt gameplay units, character dimensions, collision, saving or simulation.
Its absence retains the gameplay camera. This is preview tooling, not a
gameplay camera or a complete flicker fix.

## Renderer notes

History occlusion remains disabled by default. Visibility ties use cooked
cluster identities rather than variable selection append order. The DX12
backend automatically emits UAV barriers between dependent unordered-access
bindings. Global bias, double-sided rendering, disabled culling or forced full
detail would hide symptoms and are not used to fix camera-dependent geometry.

`OcclusionGpu::begin` classified current early bins but supplied older indirect
arguments until `retest`, after their first use. Finalizing those arguments
immediately after classification corrects missing cluster dispatches. A focused
GPU fixture fails on the old implementation and passes after the fix. See
[virtual-geometry-bin-count-stability.md](virtual-geometry-bin-count-stability.md).
Interior scene captures with this camera are not yet qualified in this repo.

## Authored camera checks

Run from the engine root:

```powershell
python tools/validation/check_scene_preview_camera.py
```

The script compiles the portable native fixture using the existing Visual Studio
environment and clang-cl, then executes it directly. It uses no GPU or CTest.
The receipt is `build/windows-x64/tools/scene-preview-camera/result.json`, with
compiler/test output in `checks.log`. It asserts authored origin independent of
player pose; retained projection and jitter; yaw/pitch/vertical movement;
normalized pitched diagonal speed; sprint and elapsed-time bounds; invalid
origin rejection; suppression of all movement and jump events; and unchanged
disabled behavior.
