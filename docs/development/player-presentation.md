# Retained player asset integration

`Rendering/Player` connects the existing `Assets/Player/octaryn_player_v1.gltf`
to standalone RHI resources and `Shaders/Player/Player.slang`. The recovered old
architecture contained player control/camera/block interaction code, but no
executable glTF import, avatar draw, or skeletal animation pipeline. This is the
missing integration for the retained asset, not a substitute model.

The asset contains 144 vertices, 216 indices (72 triangles), three material
primitives, nine nodes, eight joints and eight authored clips: idle, walk, run,
crouch walk, jump, fall, attack slash and wave. All materials are white,
metallic 0, roughness 1, alpha-mask cutoff 0.05 and double-sided. It contains no
images or textures; no replacement appearance is invented.

`PlayerModel.cpp` uses the existing fastgltf dependency to load and validate
bounded geometry, hierarchy, inverse-bind matrices, material factors and authored
animation channels. It explicitly rejects unsupported shapes/material texture
inputs instead of silently dropping them. `PlayerAnimation.cpp` samples authored
linear/step TRS keys, uses quaternion slerp for rotations, wraps looping clips and
clamps one-shots. This small eight-joint rig does not need an offline ozz asset
conversion. Local presentation samples use the authoritative source clock.
Clip changes blend local translation and scale linearly and rotation with unit
quaternion slerp, using a 120 ms smoothstep weight. Hierarchy and inverse-bind
transforms are applied after this local blend. Interrupted changes capture the
current blended pose; repeated source timestamps hold it. An explicit action
sequence restarts a one-shot's authored clock without a discontinuous pose.

Joint matrices follow the [Khronos glTF skinning description](https://github.khronos.org/glTF-Tutorials/gltfTutorial/gltfTutorial_020_Skins.html):
current global joint transform times inverse bind, weighted by each vertex's
four authored influences. Slang performs vertex skinning, inverse-transpose
normal transformation, material shading and HDR output. The CPU only samples
the small joint palette. The native and SPIR-V vertex stride is explicitly
80 bytes; shader assembly verifies offsets 0,16,32,48,64. The uniform block is
4,272 bytes with matching offsets through its final material field at 4,256.

Third-person rendering uses every original triangle. First-person rendering
removes head and torso triangles, leaving 144 indices for both arms and legs. This
is determined from joint influences: the head shares a material primitive with
both arms, so skipping an entire primitive would incorrectly remove the arms.
Torso exclusion prevents the camera-enclosing chest from covering the lower
screen; the authoritative camera and original model/root positions are unchanged.
The app owns first/third-person camera placement and animation-state selection;
the renderer receives feet position, yaw, source time and an explicit clip.
The HDR owner draws the opaque player before transparent world passes, using
shared D32Float depth with writes enabled. No network/session source is changed.

## Focused evidence

`tools/Source/ClientPlayerModelProbe/ClientPlayerModelProbe.cpp` passed against
the actual retained asset: exact geometry/rig/material counts, zero maximum
bind-pose vertex error, all eight clips moving the rig, authored loop periods,
one-shot final-key clamping, the independently decoded walk translation key,
coherent model retention
after malformed-load rejection. Transition checks cover exact endpoints, unit
midpoint rotations, non-collapsing rotated bases, interrupted movement changes,
held timestamps, completed target timing, one-shot restarts and clock resets.
Scratch objects/results are under
`work/player-model-check`. Native units compiled with the configured Windows
clang-cl warning flags; one warning originates in fastgltf's existing
`types.hpp` signed array index. Both Slang stages compiled to SPIR-V. Assembly
inspection confirms the CPU/GPU layouts and no DrawParameters capability.
The first-person refinement extends the probe to check 72 excluded head/torso
indices and all 36 indices of each original limb; the final aggregate native probe passes with 144 first-person indices.

These checks establish loading, sampling and shader contracts. They do not by
themselves prove visible GPU animation, camera composition or full presentation
parity. The parent task owns the aggregate build and actual first/third-person
capture; record those results separately when available. Multiplayer entity
replication and additional textured avatar assets are not implemented by this
bounded local-player slice.

## Integrated GPU verification

The packaged player asset path now resolves Client/Assets/Player relative to the
executable, including launches from C:/Windows. The actual asset is visibly
drawn in pipeline-third-person-final.bmp. The final first-person capture
pipeline-first-person-final.bmp confirms the head/torso obstruction is removed
while the authored arms remain visible. Both runs completed with 81 resident
columns and zero Vulkan Core/Synchronization or RHI warnings/errors.
The first-person run used the final 144-index filter; the full 216-index
third-person mesh is unchanged. Authored movement transitions are covered by
the native CPU probe; these stationary captures establish visible rendering,
not a scripted walk/jump animation sequence.

## Authoritative clip selection correction

The flight descent input no longer forces the Crouch clip outside flight. The
current server exposes no crouch pose, collider or movement state, so that input
previously produced a visual crouch while standing or falling. Clip selection
now lives in OpenWorld/PlayerPresentation.cpp; PlayerView.cpp owns the camera.
The native player probe calls the production selection function and covers
grounded idle/walk/run, descent input on ground/in air/in flight, jump/fall,
attack priority/expiry and held source clocks. These checks pass in
build/render-corrections-build.log. No server movement protocol changed.

## Stable walk/run classification

The Run cutoff was exactly 5 blocks/s, the authoritative normal walking speed
(PlayerJoltMovement.cpp uses walk 5 and sprint 9). Velocity comes from float
position differences divided by tick duration. Constant walking can therefore
produce values just above and below 5, repeatedly switching Walk/Run and
restarting animation transitions despite unchanged walking input.

PlayerPresentation now uses a named 7 blocks/s cutoff between walking and sprinting.
The production probe covers the adjacent float above 5, both sides of 7, exact 7,
sprint 9, and 64 quantized constant-walk steps across 30/60 Hz. The fixture asserts
its velocities cross both sides of 5 while the clip stays Walk. Existing idle,
air, attack and source-clock cases still pass in build/raster-culling-build.log.
Physics speeds and network interpolation are unchanged; no new moving-player
GPU capture is claimed.

## Production player GPU probe

`octaryn_client_player_rendering_probe` compiles the focused native executable.
`octaryn_validate_client_player_rendering` explicitly runs GPU validation. It
copies the executable, retained player asset, active Slang tree and compiler DLLs
into `build/<preset>/tools/validation/player-rendering`; neither target publishes
or depends on the interactive client bundle. From the repository root:

```powershell
.\tools\build\windows.ps1 -Action build -Target octaryn_client_player_rendering_probe
.\tools\build\windows.ps1 -Action build -Target octaryn_validate_client_player_rendering
```

On Windows, the GPU target requires `VkLayer_khronos_validation.json`, its DLL,
and `vk_layer_settings.txt` under `build/dependencies/vulkan-validation`. These
are explicit target dependencies: missing inputs fail instead of skipping
validation. The invocation sets process-local `VK_LAYER_PATH`,
`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` and `VK_LAYER_SETTINGS_PATH`.
The checked settings enable synchronization validation; the production RHI
device requests core validation and uses an explicit callback.

The 128x128 headless fixture calls the actual `create_player_renderer` and
`render_player` APIs with `Player.slang`, RGBA16Float HDR and D32Float depth.
It creates no SDL video device, window or presentation surface. Camera framing
comes from the retained model's vertex bounds. Hidden-player output remains
empty; the full 216-index mesh covers 1,350 pixels and the 144-index limb subset
covers 804. Switching back at the same source time reproduces the original
image exactly. The limb comparison uses the same external fixture camera;
it verifies the renderer's first-person mask, not gameplay camera placement.

For each authored clip, a fresh production animator is initialized once and
then sampled at advancing source times on that same renderer. Walk at 10.25
and 10.75 seconds changes 375 HDR pixels and 6 silhouette pixels. Attack at
10.155 and 10.465 changes 1,352 HDR pixels and 270 silhouette pixels. Both
held-time redraws are identical. Pixel assertions use actual half-float HDR
and depth readback; BMP previews apply a diagnostic tone map only for viewing.

`logs/client/player-rendering-validation.log` records the successful Windows
x64/RX 9070 XT run. The layer reports Core Checks and Synchronization enabled;
the explicit RHI/GPU callback reports zero warnings and errors, including after
resource teardown. Six `logs/client/player-rendering-*.bmp` previews show the
full model, filtered limbs, and both animation samples. This demonstrates visible
production GPU skinning and animation independently of the running game. It does
not establish multiplayer avatars, new textures, gameplay input synchronization
or other-platform runtime support.
