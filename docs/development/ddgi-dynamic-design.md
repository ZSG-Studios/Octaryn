# Dynamic DDGI for Open Worlds — Design

Status: approved direction, phased implementation. Target: AAA-quality,
always-accurate dynamic indirect lighting that streams with loaded meshes
(bundled GLB worlds today, TileSet streaming worlds next).

## Goals

- Global illumination that reacts instantly to sun/sky changes, emissive
  lights and geometry; no baked lightmaps, no per-map manual setup.
- Streaming: probe resources follow the camera through unbounded worlds;
  cost stays bounded regardless of world size (Bistro 4.15M tris today,
  tiled open worlds next).
- Convergence without lag: temporal blending with hysteresis, but never a
  smeary trail; invalid probes (moved geometry, teleports) reset hard.
- Runs on the existing Slang + standalone slang-rhi path (DX12 RT today,
  Vulkan/Metal targets later); CPU cost minimal, no readback stalls.

## Foundation already in place

- RT pipeline: TLAS rebuilt per frame (world_ray), ray lighting pipelines,
  shadow/reflection quality knobs and settings UI.
- `map_glb_load` produces world-space triangle soup + materials; images and
  primitives stream into BLAS records today.
- The dormant `Shaders/BlockTransportGI` contracts (DynamicReceivers,
  ActorHistory, Trace, PlayerOcclusion) define receiver-side slots we can
  repurpose or supersede; they must stay compilable until the new system
  replaces them.

## Architecture

### 1. Probe grid — cascaded, camera-relative

Three cascades around the camera, each an infinite probe grid conceptually:

| cascade | probe spacing | extent (XxYxZ) | probes | update rate |
|---------|--------------|-----------------|--------|-------------|
| near     | 1.5 m        | 24x16x24        | 9216   | every frame, N rays each |
| mid      | 6 m          | 24x12x24        | 6912   | every 2nd frame |
| far      | 24 m         | 16x8x16         | 2048   | every 4th frame |

Grids are camera-relative (origin snaps to spacing multiples) so streaming
is a coordinate shift, not reallocation. Storage: one RGBA16F irradiance
octahedron (8x8) + one RGBA16F radiance/visibility (8x8) + depth/state
per probe. Total VRAM ≈ 18k probes × ~1 KB ≈ 18 MB — fixed.

### 2. Probe update pass (compute, RT)

Each active probe casts 64–256 rays (cascade-dependent) with blue-noise
rotation offsets, through the existing TLAS against:
- sun/sky (direct + analytical sky model),
- emissive materials (radiance from hit),
- one indirect bounce from the previous frame's probes (feedback loop).

Irradiance is stored as irradiance-from-octahedral-direction; visibility
as mean distance + variance for screen-space weight (cracks/leak fixes).

### 3. Probe state machine (the "smart" part)

Per probe: `Active | Sleeping | Invalid | Relocated`.
- **Activity:** a probe sleeps when its radiance delta over the last K
  updates is below epsilon; waking triggers on sun delta, nearby BLAS
  updates (scene generation counter), or camera proximity. Budget: cap
  active probes per frame (e.g. 512) with round-robin fairness — steady
  state wakes ~0 probes/frame, so full-quality updates bank for changes.
- **Relocation:** probes inside geometry raycast up to 8 candidate offsets
  (DDGI paper style) and store an offset; kills the classic "probe in the
  wall" light leak.
- **Invalidation:** scene_generation bump (mesh add/remove through TileSet
  streaming or map load) marks probes by extent overlap → hard reset,
  no smearing.

### 4. Screen-space sampling (composite pass)

Pixel world position + normal → cascade select (nearest cascade whose probe
neighborhood has valid visibility) → trilinear probe blend with DDGI-style
smooth weights + chebyshev visibility test for occluder rejection. Applied
in the existing Composite/HDR path as `indirect_diffuse` alongside direct
lighting and RT shadows; fallback to current ambient when RT is off.

### 5. Integration with existing knobs

- `lighting_debug_view` gains probe visualizations (irradiance octahedra,
  probe states, update heat) — same overlay channel as existing debug views.
- Settings menu: GI quality (rays/probe, update budget), cascade distances.
- The BlockTransportGI dormant contracts retire at the end of phase 3 with
  the shader folders cleaned (single cutover, no dual paths).

## Phases

1. **Grid skeleton + debug view** — probe storage, camera-relative
   snapping, empty-update loop, on-screen probe state visualization.
   Deliverable: visible empty grid following the camera, budget HUD.
2. **RT update pass** — sun/sky + emissive radiance into irradiance,
   near cascade only, fixed full update. Deliverable: Bistro interior
   GI reacting to time-of-day (compare against current ambient path).
3. **Temporal + state machine** — sleep/wake/relocate/invalidate,
   budgeted updates, all cascades. Deliverable: bounded frame cost at
   4.15M tris, no leaks on the Bistro interior, hard reset on scene edits.
4. **Sampling integration** — composite indirect_diffuse with visibility
   weights, quality settings, debug views. Deliverable: side-by-side with
   the current direct-only path, validated by captures.
5. **Streaming worlds** — probe invalidation hooks into TileSet mesh
   add/remove; far cascade driven by tile load radius.

## Risks / open questions

- DX12 RT ray budget at 4.15M tris: measure phase-2 costs early; ray count
  per probe is the main knob.
- Metal/Vulkan inline RT support differs; keep the update pass behind the
  existing ray-availability checks.
- Probe feedback (one-bounce) can ring; clamp radiance gain per update.
