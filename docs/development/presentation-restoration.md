# Restore original presentation behavior

The recovered original implementation defines the restoration contract. The
initial minimal WorldRenderer was a bring-up step; the current integrated
presentation checkpoint and remaining differences are recorded below.

## Current checkpoint: standalone RHI presentation

The current active pipeline uses standalone shader-slang/slang-rhi and Slang
shaders for sky, four-target G-buffer, HDR resolve, skinned player, clouds,
glass/water/lava, selection, tone mapping, RmlUi and presentation. The older
GFX checkpoint below is historical. See [pipeline-parity.md](pipeline-parity.md)
for current GPU validation, source mapping and unresolved algorithm limits.

The player asset now loads from the actual packaged Client/Assets path.
F4 toggles the collision-aware third-person camera; first person removes
head/torso triangles that intersect the eye while preserving authored limbs.
RmlUi replaces the previous shader-font/ImGui implementation at the user's
separate request, retaining existing settings owners and Slang RHI submission.

Final fixture captures and CPU/GPU oracles pass, including all 18 fluid cases
and strict Vulkan Core/Synchronization/RHI validation through destruction.
The Windows 1280x720 fixed-fixture benchmark averaged 0.876ms, worst 2.335ms
over 15 measured seconds after 5 seconds of warmup; this is not moving-world or
cross-platform qualification. Detailed evidence is in the pipeline parity record.

## Recovered source of truth

The development tarball omitted `references/old-architecture`. The original
repository at https://github.com/ZSG-Studios/Octaryn.git contains this reference
and its Git history. It is now available in the separate read-only checkout
`ref/upstream-octaryn`, inspected at commit
`3557cbfdc803ec034122bb55070b62b3b43b5588` (401 old-architecture files).

Paths beginning `original/` below mean
`ref/upstream-octaryn/references/old-architecture/source/`.

## Recovery map

| Area | Original implementation | Required active integration |
| --- | --- | --- |
| Frame orchestration | `original/app/runtime/iterate.cpp`, `original/app/runtime/render/render.cpp`, `original/app/runtime/render/passes.cpp` | Reconnect owner update, atlas animation, render context, scene passes, overlay, submit and profiling. Preserve server authority rather than restoring old local-world mutation. |
| Scene passes | `original/render/scene/`, `original/render/pipelines/`, `original/render/resources/` | Restore G-buffer sky/opaque/sprites, composite, forward transparency/water/lava/clouds, UI and presentation in the old order using standalone Slang RHI and Slang shaders. Do not expand the flat-color path into a separate competing renderer. |
| Textures and materials | `original/render/atlas/`, `original/shaders/`, `octaryn-basegame/Assets/Atlases`, `octaryn-client/Shaders/Materials` | Reuse original atlas layer mapping, color/normal/specular sampling, mips, alpha cutoff, animation and material contracts. Port exact shader behavior to Slang and bind it in the active scene passes. |
| Lighting | `original/app/lighting/settings/`, `original/app/lighting/ui/`, original composite and shared shaders | Recover the implemented behavior and settings first. Current original composite uses ambient/emission; some skylight integration was already disabled. Distinguish complete algorithms from wired runtime behavior. No new DDGI redesign is authorized by this restoration request. |
| UI | `original/render/ui/ui.cpp`, `original/shaders/ui.comp.glsl`, `original/app/overlay/`, `original/app/lighting/ui/`; active `Ui/RuntimeControls`, `Ui/DisplayMenu`, settings owners | Port original pixel-font/menu draw behavior to Slang and reconnect existing state/action/settings owners. Preserve actual key/action semantics and the original lighting panel instead of inventing replacement menus. |
| First-person player | `original/app/player/player.cpp`, `blocks.cpp`, `physics/`; current server player and block authority owners | Restore targeting, selection, break/place/cycle, placement exclusion and feedback/audio through authoritative commands. Keep current server physics, timing and persistence fixes. |
| Humanoid model | `octaryn-client/Assets/Player/octaryn_player_v1.gltf`, fastgltf/ozz dependency wrappers | The retained asset now has an active fastgltf importer, authored animation sampling with 120 ms local-TRS transitions and RHI GPU skinning. This fills a missing owner; the original executable did not draw this asset. First-person head filtering and F4 third-person camera are wired. See player-presentation.md for tests; remote avatars still require authoritative entity lifecycle/pose contracts. |

## Acceptance and order

The first texture slice must preserve these inspected contracts: 29 atlas layers
of 32-square pixels, six mip levels, sRGB albedo, linear normal/specular data,
cutout alpha coverage and animation timing. Read layer assignments from existing
basegame block data/generated catalogs. Current face directions
`[-X,+X,-Y,+Y,-Z,+Z]` map to original catalog indices `[3,2,5,4,1,0]`.
Grass top, side and bottom use different layers. Active material sampling,
LabPBR decoding, terrain parallax and original ambient/emission math have now
been ported to Slang. The restored HDR/forward graph is now wired on standalone Slang RHI; see
[pipeline-parity.md](pipeline-parity.md) for current verification and limits.

1. Reconnect original atlas/material contracts and animated textures, with source
   references and captured runtime evidence. The first runtime slice is verified below.
2. Reconstitute the original scene-pass/resource flow and implemented lighting
   behavior in existing client owners, converting shader language only as needed.
3. Reconnect original UI/settings actions and first-person interaction feedback.
4. Integrate the retained player asset and entity presentation where implementation
   is missing, explicitly separating this work from recovered old behavior.
5. Remove temporary flat-color scaffolding after the corresponding restored path
   has actual runtime evidence. Do not remove working baseline behavior first.

Each feature requires an original-source pointer, active owner call path, runtime
capture/check, and a candid list of remaining differences. Dependencies, copied
assets, compiled helper shaders and probe output are not feature completion.

Use CLI tooling and renderer screenshots only. No app control or UI automation.

## Historical GFX checkpoint (superseded)

The backend is Vulkan through **Slang GFX (`gfx::`)**, not standalone `slang-rhi`.
Existing SlangRhi target/file names do not establish the latter integration.

- `Rendering/Atlas` loads the original 29-layer albedo/normal/specular assets,
  six mips, original filtering/alpha coverage, samplers and four animations.
  Runtime face-to-layer mapping comes from the basegame catalog.
- `Rendering/Sky` ports original sky gradient, stars, sun/moon and day/night
  settings. Coherent server world-time fields drive the sky and ambient scales.
- GPU count/emit keeps five contiguous indirect ranges per retained column:
  opaque/cutout, sprites, glass, water, lava. Submission follows the original
  opaque/sprite then glass/lava/water order. Crossed flowers and thin torch
  geometry are ported. Transparent columns sort far to near.
- Existing `Ui/RuntimeControls` and display/settings owners are connected to the
  app; the original 15-row pixel menu, HUD, crosshair and block preview use Slang.
  F/F5 flight, Z 1/2/4 zoom, F3 debug, F11 fullscreen, Escape settings and
  authoritative break/place/pick/cycle are connected. Render distance is visibly
  limited to the currently supported radius 4 (81 columns), not silently 32.
- Existing Camera frustum culling is reused. The independent clip-space oracle
  covered 20,790 views and 3,596,670 boxes with zero false culls. A fixed-camera
  terrain capture compared 834,700 pixels with culling on/off, all identical.

Evidence: `logs/client/presentation-fixture-validation.log` exits zero after
1,168 frames with zero Vulkan Core/Synchronization errors or warnings, including
destruction. `presentation-fixture.bmp` and its GPU `.quads.bin` contain 81
columns and 3,353,359 valid faces. The fixture visibly exercises torch, flower,
glass and separated fluid surfaces. `presentation-menu-validation.log` and
`presentation-menu.bmp` prove the settings overlay in the actual Vulkan frame.
The sky pipeline's device-before-resource destruction error was fixed and the
later fixture/menu validations cover the corrected lifetime.

This is **not full 1:1 parity**. The old four-MRT G-buffer/HDR composite remains
fused into current raster/presentation math. Original water/lava corner heights,
flow, tint/absorption/Fresnel/waves and HDR forward composition are not restored;
water visibly remains gray. Clouds, full lighting-panel wiring, selection/audio
feedback, humanoid import/animation/skinning, transport and desktop qualification
remain incomplete. The original fog menu existed, but original general composite
fog was itself unwired; distinguish that from implemented fluid fog math.

Next: restore the original G-buffer/HDR and forward fluid responsibilities, then
clouds/lighting controls and remaining player feedback. Preserve the verified
atlas/UI/authority work and do not replace it with another demonstration path.
