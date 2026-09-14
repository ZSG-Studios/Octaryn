# Active Slang/RHI presentation pipeline

Updated 2026-09-13. Source mapping below refers to the recovered, read-only
`ref/upstream-octaryn/references/old-architecture/source` at upstream commit
`3557cbfdc803ec034122bb55070b62b3b43b5588`. “Wired” describes current source and
CMake ownership; full-frame runtime evidence is recorded separately below.
It does not mean every retained engine feature or desktop platform is complete.

The active client uses standalone shader-slang/slang-rhi commit
`e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc`, static Vulkan-only build, and Slang
2026.17.1. SDL owns window/events; it does not submit this renderer's GPU work.
First-party render passes use `rhi::` resources/encoders and authored `.slang`
shaders. Vulkan calls inside the upstream RHI implementation are expected.

## Source-to-runtime map

Paths in Current are relative to `octaryn-client/Source` or `octaryn-client/Shaders`.

| Responsibility | Recovered source | Current owner and shader | State / remaining difference |
| --- | --- | --- | --- |
| Device, surface, resources, submission | `render/pipelines`, `render/resources`, SDL GPU shader/pipeline calls | `Rendering/RenderBackend/{WorldRendererDevice,WorldRenderer,RhiShader}.cpp` | Standalone RHI device explicitly selects Vulkan; shared Slang program compiler and RHI state tracking. No active GFX wrapper. |
| Texture arrays and animation | `render/atlas`, original material sampling helpers | `Rendering/Atlas/*`, `Materials/{MaterialSampling,TextureArrays,LabPbr,TerrainParallax}.slang` | Actual basegame albedo/normal/specular assets, native animation frames, alpha handling and material flags. |
| Terrain visibility and geometry | Original world chunk meshing/culling and indirect draws | `Rendering/RenderBackend/{WorldRendererMesh,WorldDraw}.cpp`, `Voxel/WorldFaces.slang`, existing Camera owner | Active bounded GPU face generation/indirect draws and camera frustum culling. This is not the entire preserved old GPU greedy/prefix/visibility pipeline. |
| Opaque and crossed sprites | `render/scene/gbuffer.cpp`, `shaders/opaque.frag.glsl`, `sprite_packed.vert.glsl` | `Voxel/{WorldRaster,WorldSprites}.slang` | Four attachments restored: RGBA16F albedo, RGBA16F camera-relative position, RGBA8 voxel, RGBA8 material. PBR/POM flags and original ambient/emission contract are wired. |
| Sky gradient, stars, sun, moon | `render/scene/gbuffer.cpp`, `shaders/sky.{vert,frag}.glsl` | `Rendering/Sky/{SkyData,SkyRenderer}.cpp`, `Sky/{Sky,SkyRay,Atmosphere,Celestial}.slang` | Original linear LDR sky writes only the albedo attachment before opaque MRT. Gradient/star/sun/moon controls retain original math; full-screen ray reconstruction replaces the enclosing cube geometry. |
| Day/night and animation time | `core/world_time`, `app/runtime/render/context.cpp` | Server world-time snapshot → `LocalPlayerPose` → `SkyData` | Authoritative day fraction controls celestial orbit/visibility. Animation uses presentation source seconds modulo 65536, matching original real-time animation rather than accelerated world-clock seconds. |
| Block clouds | `shaders/clouds.frag.glsl`, `shader_common.glsl`, `render/scene/forward.cpp` | `Rendering/Sky/CloudRenderer.cpp`, `Sky/{Clouds,CloudColor,SkyRay}.slang` | Original height 192.33, thickness 1, 3-unit cells, drift 0.6, alpha 0.72, weather/noise thresholds and 1024-step bound. HDR alpha-blended pass before glass/lava/water; depth-test on, depth-write off. Menu toggle is wired, extent is render radius × 32 × 2. Clouds are visible in the final first- and third-person captures. |
| Ambient/HDR resolve | `shaders/composite.comp.glsl` | `Rendering/Hdr/WorldHdr.cpp`, `Hdr/Composite.slang` | Restores inverse tone mapping for sky and original ambient + emission resolve into RGBA16F. Completes the previously unused Sun control with unshadowed Lambert diffuse on terrain; this added wiring does not establish shadows, visibility-tested sunlight or fallback lighting. |
| Fog | Original `shader_common.glsl` helpers; `transparent.frag.glsl` fixed-distance fluid haze | `Sky/Fog.slang`, `Hdr/Composite.slang`, `Voxel/WorldRaster.slang` forward entry | Original color/falloff helpers ported. Global fog toggle/distance now wired into opaque/HDR and forward terrain: this completes previously unused original controls. Preserved weak fluid-specific haze remains separate. |
| Glass, lava, water | `render/scene/forward.cpp`, `shaders/transparent.frag.glsl`, fluid vertex helpers | `Voxel/{WorldFluidTypes,WorldFluidMesh,WorldFluidShade,WorldRaster}.slang` | Separate HDR forward passes, original fluid surfaces/UV/animation/tint and far-to-near column ordering. Sun strength now scales the water specular highlight. Full transparency ordering remains approximate at column granularity, as in the original owner. |
| Player model and authored animation | Retained `Assets/Player/octaryn_player_v1.gltf`; old executable contained camera/controller only | `Rendering/Player/{PlayerModel,PlayerAnimation,PlayerRenderer}.cpp`, `Player/Player.slang` | New missing-owner integration of the actual asset: 144 vertices, 72 triangles, 8 joints, 8 clips; GPU skinning and authored TRS sampling. Three white materials have no image textures in the asset; no replacement skin was invented. Direct sampling currently uses fastgltf; ozz is not used for this tiny authored rig. First person removes head and torso triangles, drawing 144 indices; third person retains all 216 indices. The final first-person capture no longer has the foreground body slab. Source-time transitions now smoothstep over 120 ms, blending authored local translation/scale and quaternion slerp before hierarchy evaluation; interruption continuity and held clocks are covered by CPU tests. Multiplayer avatar replication is not implemented. |
| Target highlight | Original selection shaders/pipeline and authoritative targeting intent | `Rendering/Selection/SelectionRenderer.cpp`, `Selection/Selection.slang` | Selection draws through RHI; block edits remain server-authoritative. |
| Tone map / output | `shaders/present.frag.glsl` | `Hdr/Present.slang`, original transfer helpers | HDR is tone-mapped and converted to output color once, before UI. |
| Settings/debug/HUD | `shaders/ui.comp.glsl`, `app/overlay` | `Ui/GameUi/*`, `Rendering/Ui/RmlRenderer.cpp`, `Ui/Rml.slang`; existing `Rendering/Ui/UiData` and RuntimeControls data owners | User-requested RmlUi conversion is implemented and packaged. RML documents own HUD/menu content; the custom Rml render interface submits geometry through standalone RHI and Slang. Existing display/control state and unavailable-telemetry semantics are retained. Four captured HUD/settings/lighting/diagnostic sessions passed document contracts and GPU validation; the former compute overlay is removed. |
| Lighting tuning UI | `app/lighting/ui`, original ImGui controls | `Ui/GameUi/*`, `Ui/LightingPanel/LightingPanel.cpp` settings owner, `Rendering/Ui/RmlRenderer.cpp`, `Ui/Rml.slang` | Lighting controls use the same RmlUi document/render path with F6 and existing settings persistence. The former ImGui renderer/shader is removed. Captured lighting/layout contracts pass; a separate same-camera pixel comparison verifies the terrain Sun control changes actual output. Unsupported lighting algorithms remain listed below. |

## Current gaps and limits

- Terrain now has unshadowed Lambert Sun lighting, and Sun strength reaches the
  water specular highlight and player material. Shadowing, visibility-tested
  sunlight, fallback direct light, DDGI and ray tracing remain unimplemented. A
  compiled `Lighting/DDGIReady.slang` helper is not a functioning GI pipeline.
  The terrain Sun response is verified by the pixel comparison below; this is
  not evidence of shadows or full PBR lighting.
- Full terrain PBR remains incomplete: material/POM decoding, original
  ambient/emission and unshadowed geometric-normal Sun response do not establish
  a complete normal-mapped BRDF, indirect light or environment-lighting pipeline.
- Remote-avatar replication and presentation are not implemented. The verified
  skinned model belongs to the local authoritative session.
- The original gameplay skylight lookup functions in `shader_common.glsl` return
  constant values. Preserve this provenance: a restored hemisphere factor is not
  proof of authoritative voxel occlusion lighting.
- The active interactive radius remains bounded (currently four chunks each
  direction). Supporting the original 128-option distance menu at full scale
  requires separate streaming/performance work; the menu is capped to the actual
  supported value rather than silently displaying an unapplied setting.
- Original GFX probe implementations remain dormant source references outside
  the active graph. Their ten old executables were preserved outside the repo
  and removed from active build outputs. They are not counted as migrated GPU
  algorithms, and the active-path validator rejects reconnecting them unchanged.
- Native Windows x64 Release is the qualified platform baseline. Linux, macOS,
  ARM64, HDR displays and VR are not established by these Windows tests.

## Verified integrated Windows runtime

The final packaged client runs the combined HDR, player, cloud, fluid, selection
and RmlUi path through standalone Slang RHI/Vulkan on AMD Radeon RX 9070 XT.
The following captured runs enabled Vulkan Core and Synchronization validation
and RHI validation. Each exited successfully with zero GPU warnings/errors:

| Evidence under `logs/client/` | Result |
| --- | --- |
| `pipeline-third-person-final.log` and `.bmp` | 3,078 fully resident frames, 81 columns and 1,484,983 faces. Actual third-person skinned model, terrain, clouds and RmlUi are visible. |
| `pipeline-first-person-final.log` and `.bmp` | 3,113 frames, 81 columns. The latest 144-index head-and-torso filter clears the foreground body slab; clouds and the first-person scene remain visible. |
| `pipeline-fluid-levels-final.log` and `.bmp` | 3,214 frames, 81 columns; the expanded fluid-level fixture renders through the integrated pipeline. |

These runs establish the tested scenes and platform, not a manual play-through
of every control or complete engine feature parity.

## Geometry, lighting and UI evidence

- `logs/client/pipeline-fluid-levels-oracle.log`: all 18 fixture cases and every
  fluid level 0 through 7 passed. The capture contains 60 fluid faces (30 water,
  30 lava); maximum absolute geometry error is 5.960464477539063e-8, zero errors.
- `logs/client/pipeline-fixture-geometry.log`: all five material/geometry cases
  passed with 17 expected fixture faces, within the 81-column captured world.
- `logs/client/pipeline-lighting-pixels.json`: with the same camera and authored
  stone surface, ambient set to zero and Sun changed from 0 to 3, 58,500 compared
  pixels changed mean display luminance from 0.1124849930 to 0.4528745215.
  This verifies actual terrain Sun output, not merely uniform or slider changes.
- `logs/client/cloud-fog-check.log`: headless RHI compute execution of production
  cloud/fog functions passed 4,096 signed-coordinate cells (507 filled, 3,589
  empty), six intersection/direction/range cases, top/bottom depths and normals,
  fog enable/disable and the original 2.5-power falloff with zero errors. The
  captured full-frame runs above additionally exercise visible cloud blending.
- [RmlUi qualification](rml-ui.md) records four separate HUD, settings, lighting
  and diagnostic captures, each with 600 fully resident frames and 81 columns.
  Each passed 543 document/binding/slider/focus/layout checks across three
  resolutions, with stable retained geometry and no RmlUi/RHI warnings/errors.
  The earlier body-geometry issue mentioned there is resolved by the newer
  `pipeline-first-person-final` head-and-torso-filter capture above.
- Player CPU checks cover the actual eight-clip asset, 120 ms source-time local
  TRS transitions, interrupted transitions, held clocks and one-shot restarts.
  See [player presentation](player-presentation.md) for the authored asset and
  skinning contract; captured first/third-person evidence is listed above.

## Measured performance and policy checks

- `logs/client/pipeline-benchmark-final.log` and `.csv`: 1280x720, fully resident
  81-column scene, five-second warmup and 15-second measurement, without GPU
  validation or capture. Across 17,114 measured samples, mean frame time was
  0.876 ms, worst 2.335 ms and histogram-derived 1% low 666.67 FPS. This is a
  bounded benchmark of that scene on this machine, not a guarantee for other
  worlds, resolutions, machines or long sessions.
- `logs/client/pipeline-shaders.log`: all 49 Slang sources, 30 complete modules
  and 28 distinct entry/stage combinations compiled successfully. Annotated
  entries, including `WorldRaster.forward_main` and RmlUi's vertex/fragment
  entries, are discovered automatically. Shared Sky/Cloud entries compile
  through their declaring parents; four unannotated historical entries remain
  explicitly covered.
- `logs/client/pipeline-audit.json`: PASS for 73 active first-party sources,
  65 included headers and 24 reachable shader modules. The validator queries
  Ninja's actual client closure and compile commands, verifies standalone RHI
  linkage/Vulkan selection, and rejects active GFX, raw Vulkan, SDL GPU rendering,
  OpenGL and GLSL. Upstream backend implementations and dormant source files are
  distinguished from first-party active code. This is a source/build-graph
  policy check; runtime evidence is recorded separately above.
- The packaged shader tree equals the current source tree. Bundle policy now
  requires `Ui/Rml.slang` rather than removed Overlay/ImGui/Glyphs/Text sources.
- Pipeline-policy self-tests passed one positive and ten negative API/link/include
  cases. Shader-entry parser checks cover four entry names, two comment decoys,
  legacy unannotated declarations and two invalid annotations. Bundle negative
  checks reject missing modules, stale contents and legacy GLSL injection.

No source edits, build or GPU execution were performed during this final
documentation update; the results above record the completed run artifacts.

## Follow-up corrections and bundle recovery

The subsequent native build and complete client/server bundle pass in
build/render-corrections-build.log and build/render-corrections-bundle-final.log.
The production player-presentation probe now also rejects false crouch from the
flight descent key; no authoritative crouch state exists in the current server.
Sprite normal/specular sampling now uses the original inset-clamped LOD0 atlas
path, matching sprite albedo. Terrain retains gradient-based mip sampling.
Both changed raster stages pass SPIR-V validation for Vulkan 1.2 in
logs/client/sprite-material-parity.log. All 49 shader sources, 30 modules and
28 entries compile in logs/client/render-corrections-shaders.log.
RmlUi retention reports are bounded to one normal-run line or validation frames
1, 301 and 601, with unconditional error reporting.

The updated active-graph audit passes 74 sources, 65 headers and 24 reachable
shader modules in logs/client/render-corrections-audit.json. Packaged shaders
match source exactly. The shader policy continues to reject active legacy APIs.

An attempted rebuild exposed the old recipe's destructive pre-clean: locked
runtime DLLs prevented complete removal after unlocked files had been removed.
All missing files were restored from current build owners and verified against
a subsequent complete staged publish. The bundle installer now builds the entire
client/server payload in bundle.staging, skips identical content, and retains
the previous bundle under bundle.retired when replacing it. Rename failures
preserve or roll back the old directory; a rollback failure reports its retained
recovery location. Ten temporary-directory tests pass, including a real Windows
sharing lock and injected install/rollback failures. The final build reports
retained=unchanged, proving the recovered canonical bundle equals the full
publish. The running client, PID 13560, remained responsive and was not stopped.

Review also added the managed bridge as an explicit bundle file dependency;
Ninja confirms the DLL is a normal input, so bridge-only relinks trigger
repackaging. Identical-content comparison includes file/directory permission
bits. Its POSIX executable-bit regression is skipped on this Windows host:
ten tests pass and one platform-specific test is skipped, not qualified here.
The post-review full bundle rebuild also passes with retained=unchanged in
build/render-corrections-bundle-reviewed.log. Source/bundle shader equality and
the active backend audit were rechecked after that build; native and packaged
client executable SHA-256 hashes match. All 709 code files scanned under active
owner/cmake/tools roots remain within the 500-line limit.

These follow-up changes have native, shader and packaging verification. No new
GPU run or benchmark was started alongside the user's running client. The
captures/performance above predate these corrections; the updated executable
and shader files apply on the next launch. The previous executable and hash
manifest remain in Octaryn-Backups/2026-09-13-render-corrections.

## Source-time presentation and shared draw preparation

The next pass fixes future-snapshot velocity/contact metadata reaching the
player before the interpolated position reaches that snapshot. See
networking-recovery.md for the exact cursor contract and CPU regression cases.
Player clips now consume velocity interpolated at the presentation cursor and
discrete state from the last reached authoritative tick. The camera position
algorithm and server protocol are unchanged.

WorldRenderer now prepares its 36-float uniform block, camera/frustum, visible
column list and stable far-to-near order once per frame after mesh refresh.
Opaque and forward passes consume that same retained list. Draw order, tie
ordering, bindings, fluid offsets and counters are preserved. The vector keeps
capacity between frames. This removes duplicate preparation and fresh vector
growth, but does not claim all stable_sort implementations are allocation-free.

The canonical octaryn_validate_client_draw_preparation target calls the actual
CPU preparation function without creating a device or window. All 116 checks
pass: culling/front/behind/inside/empty columns, signed-coordinate distance ties,
zoom and portrait dimensions, forward-only statistics, 100 storage-reuse frames,
and erase/insert/bounds replacement/empty-frame refresh. The player probe also
passes its new pose_metadata checks and existing asset/animation checks.
Evidence: build/presentation-timing-build.log. This pass does not establish a
new full-frame FPS measurement or updated GPU capture.

Native default world selection now matches tools/run-client.ps1 at
saves/open-world-v2, including direct executable and build-tool launches.
Explicit OCTARYN_CLIENT_WORLD_PATH overrides remain supported. The existing
revision-2 metadata was checked; older saves were neither renamed nor migrated.
The native default-path change was compiled, not separately launched here.

The first full publish safely failed when Windows denied renaming the in-use
bundle. SHA-256 comparison showed only Octaryn.Client.exe differed from staging.
That single file was backed up under Octaryn-Backups/2026-09-13-presentation-timing
with a hash manifest, then atomically replaced; runtime DLLs were unchanged.
The final complete publish passes with retained=unchanged in
build/presentation-timing-bundle-final.log. PID 13560 remained responsive and
continues its already-loaded code; the corrections apply on the next launch.
The source/bundle shader comparison passes and the active Slang RHI graph audit
passes 74 sources, 65 headers and 24 reachable shader modules in
logs/client/presentation-timing-audit.json. All 711 scanned owner/tool/CMake code
files remain within 500 lines. No shader source changed in this pass.

## Edge-column restoration after rapid boundary reversal

The next pass fixes the mismatch between immediate renderer eviction and
worker-only completion retirement. A rapid center A/B/A change now invalidates
the edge column's completion metadata synchronously, allowing its unchanged
revision to be delivered again. Large payload disposal remains on the worker.
See networking-recovery.md for the race, preserved invariants and deterministic
red/green regression. The actual async stream probe passes, including 24,480
terrain-parity samples. Native checks pass in build/stream-residency-build.log.

Mouse look again uses the original app/player/player.cpp sensitivity of 0.1
degree per SDL relative count, converted to radians for current camera state.
The prior 0.0025 radians/count rotated 100 counts by 14.3239 degrees instead of
10 degrees. Existing pitch bounds and event/capture behavior are preserved.
This is a compiled source-parity correction, not a synthetic-input/UI test.

Final packaging passes in build/stream-residency-bundle-final.log. The loaded
bundle's directory rename was denied safely; only the executable differed from
the complete staged publish. Its predecessor and hash manifest were preserved
in Octaryn-Backups/2026-09-13-stream-residency before atomic replacement. The
subsequent complete publish confirms identical installed/staged contents.
PID 13560 stayed responsive; changes apply on its next launch. No new shader,
GPU capture or FPS result is claimed. Backend audit passes 74 sources, 66
headers and 24 reachable modules in logs/client/stream-residency-audit.json;
shader bundle equality and owner-boundary checks pass.

The visible render-distance menu is currently fixed at its only supported
option, four chunks; its startup capture is a future integration concern, not
an ignored currently changeable setting. Action sounds are a confirmed missing
original owner; action-audio.md maps their source and next bounded restoration
work. Audio has not been implemented or qualified by this pass.

## Follow-up action feedback and removed-block audit

Original action audio is now implemented and passes native compilation, action
hook checks, 42 CPU checks and 49 actual OpenAL loopback checks. This supersedes
the preceding audio absence statement. See action-audio.md for ownership,
original event semantics, lifecycle handling and coverage limits. The complete
staged bundle passes content/module/server/shader/hash validation. Canonical
installation is pending the running game's file lock; audio is not claimed
installed or tested through actual packaged gameplay actions.

The refreshed active rendering audit passes 78 sources, 69 headers and 24
reachable Slang modules. Source and staged shader trees match; there is no
active GFX runtime. The audio slice changes no GPU shader or submission path.
Earlier first/third-person and integrated GPU captures remain the rendering
evidence; they predate this CPU/audio slice.

The old engine's world/edit/hidden_blocks.cpp hid changed non-air voxels until
the replacement mesh epoch uploaded, with a five-second watchdog. Its
app/runtime/render/context.cpp supplied up to 32 coordinates to the opaque/sprite
G-buffer pass; glass and fluids did not consume them. Current Scene/HiddenBlocks
helpers are not consumed by WorldRaster/WorldRenderer. Initial inventory called
this a confirmed missing visual behavior, but the delivery trace corrects that
conclusion: OpenWorld polls a column and completes its GPU mesh replacement
synchronously before querying interactions or drawing the next frame. Adding
the old mask only during that operation would not change a displayed frame.

Earlier worker publication of CPU query data is a separate coherence concern:
the original implementation updated query_columns before its ready queue was
polled, allowing camera/targeting to observe not-yet-delivered geometry. Address
that at the delivery boundary instead of adding a speculative shader path.
Stream revisions are content hashes, not ordered mesh epochs; never compare
them with >= as a substitute for original upload generations.

The subsequent CPU repair publishes the exact immutable query column only when
its paired render payload is delivered. OpenWorld completes the GPU replacement
before targeting/camera work, preserving visible geometry/query agreement.
Sticky visibility invalidation prevents old query data returning after an
away/back eviction; bounded worker retirement avoids adding large query frees
to the frame thread. Production-owner red/green regression and the real threaded
stream probe pass, including unchanged 24,480 terrain-parity samples. See
networking-recovery.md. This fixes the demonstrated coherence issue without
adding a masking shader or changing the existing Slang RHI render passes.

## Original opaque culling and depth comparison restored

The shared opaque/sprite pipeline inherited RHI's default CullMode::None and
used Less depth comparison. Recovered render/pipelines/graphics_mesh.cpp instead
uses Back/CounterClockwise/LessEqual for opaque geometry and None/LessEqual for
sprites. The focused WorldRasterPipeline owner now creates the actual production
pipeline family, and WorldDraw selects separate opaque/sprite states. Forward
lava/glass/water keep None/Less, existing depth-write rules and blending. Shaders,
atlas bindings, face data, draw order and indirect offsets are unchanged.

The headless octaryn_validate_client_raster_culling target calls that production
helper, actual atlas owner and world_renderer_prepare_draw/world_renderer_draw.
It uses 64x64 four-format G-buffer attachments plus D32, actual Slang WorldRaster
entries and indirect face draws, with no window/surface. GPU readback proves:

- All six stone directions: 784 exterior pixels and zero interior pixels each.
- All four authored bluebell face records: 116 pixels on each side.
- Ten equal-depth redraws preserve exact coverage after clearing color while
  retaining depth, verifying LessEqual for both opaque and sprite families.

The canonical GPU target passes in logs/client/raster-culling-gpu.log. A second
process-local Khronos Core/Synchronization + RHI validation run also passes in
raster-culling-validation.log, with no GPU validation errors or warnings. Loader
diagnostics confirm the layer and include notices about explicit environment
activation; these are not rendering validation failures. The adapter is AMD
Radeon RX 9070 XT. Earlier full-world captures predate this state correction;
no new full-frame FPS result is claimed.

The native build passes player animation quantization regressions and all 116
draw-preparation checks in build/raster-culling-build.log. The complete 151-file
stage passes module/server/shader/hash, owner and 726-file line-limit checks in
logs/client/raster-culling-stage.json. The active RHI graph now passes 79 sources,
70 headers and 24 shader modules. Canonical installation remains pending the live
game lock; its session stayed responsive during these isolated checks.

## Selective neighbor mesh invalidation

Every delivered column previously scheduled all eight resident neighbors for GPU
remeshing, even when only an interior voxel changed. WorldMeshInvalidation now
compares the old and new facing boundary strips/corners before retained-source
replacement. This matches the actual one-cell input dependency in WorldMeshHalo,
WorldFaces and WorldFluidMesh, including all Y samples and diagonal fluid corners.
It preserves existing dirty entries. First load, unload and changed vertical
extent retain conservative invalidation; no revision-hash equality is assumed.

For a same-extent edit with all neighbors resident, scheduled neighbor rebuilds
fall from eight to zero for an interior cell, one for a noncorner edge, or three
for a corner. The changed column still rebuilds normally. Original
world/edit/schedule.cpp used boundary-gated cardinal updates; the current fluid
shader also needs diagonal samples, so its corner dependency remains covered.

The CPU probe compares actual world_mesh_halo vectors before and after edits,
not a duplicate implementation of the selector. All 3,264 checks pass across
1,024 horizontal positions, full-height/top/bottom edits, signed coordinates,
metadata-only changes, existing pending work, missing recipients, arrivals,
vertical extents and actual set_center unloads. Every skipped neighbor's input
is unchanged; conservative lifecycle cases may still schedule unchanged inputs.
The existing 116 draw-preparation and player tests also pass in
build/halo-invalidation-build.log. No GPU device or window is used by these tests.

Final staging and owner checks pass for 151 payload files and 728 code files;
the active Slang RHI graph passes 80 sources, 70 headers and 24 shader modules
in logs/client/halo-invalidation-stage.json and halo-invalidation-render-audit.json.
Canonical installation remains blocked by the existing running game, with the
previous coherent bundle retained. No shader changed and no new FPS improvement
is claimed. The bounded live profile tail was stationary with zero streaming
work, so it is not evidence of improved edit-time performance.

## Ordered input and action feedback follow-up

Controls now preserves the accepted order of up to 64 block actions per frame.
The original place-then-wheel behavior and repeated clicks are retained, and
feedback follows successful dispatch order without implying server application.
Existing capture/modal rules and the single per-frame attack sequence update
remain intact. This changes CPU input dispatch, not shaders or render passes.

The native application links and the canonical CPU player-model probe reports
action_feedback=passed alongside its existing checks in
build/ordered-actions-build.log. See input-ordering.md for queue overflow policy,
original behavior and precise probe scope. The complete 151-file stage passes
in logs/client/ordered-actions-stage.json, with 730 code files within the limit.
The active graph passes 80 sources, 71 headers and 24 reachable shader modules;
all 49 Slang sources, 30 modules and 28 entry/stage combinations freshly compile.
Canonical installation stops safely at the running game's directory lock in
build/ordered-actions-bundle.log. No SDL injection, new gameplay run or
performance gain is claimed for this input follow-up. The separate player GPU
fixture below establishes its own bounded rendering evidence.

## Headless production player visibility and animation

The explicit `octaryn_validate_client_player_rendering` target now qualifies
the actual PlayerRenderer/Player.slang path on standalone RHI Vulkan, using the
retained glTF and a 128x128 RGBA16Float/D32Float offscreen fixture. The separate
`octaryn_client_player_rendering_probe` target only compiles it. Assets, shaders
and compiler runtimes are staged under tools/validation/player-rendering; no
interactive bundle or running game is modified.

The hidden baseline is empty; the full model covers 1,350 pixels and the
head/torso-filtered limbs cover 804 from the same external camera. Walk changes
375 HDR pixels between authored times and Attack changes 1,352; silhouette
changes are 6 and 270 respectively. Held clocks and restoration of full-mesh
mode reproduce identical readbacks. These are actual production GPU results,
not a substitute skinning shader or CPU-only animation comparison.

Evidence is logs/client/player-rendering-validation.log and six associated BMP
readbacks. The Windows target requires the local Khronos validation manifest,
DLL and synchronization settings as explicit prerequisites. The layer confirms
Core Checks and Synchronization enabled; the GPU callback reports zero warnings
and errors through teardown. See player-presentation.md for commands and scope:
the filtered-limb fixture is not a new gameplay first-person camera test, and
remote avatars, additional textures and other-platform support remain unproven.
