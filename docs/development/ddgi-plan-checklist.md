# Voxel DDGI plan checklist

Status of `Voxel DDGI-PLAN.txt` against the active renderer. Source of truth for
remaining DDGI work; update this file when a section changes state.

Legend: **Done** = implemented and verified in the active build, **Partial** =
exists but does not meet the plan's full bar, **Gap** = not implemented,
**Deferred** = intentionally later (plan says so or dependencies missing),
**Divergent** = plan item conflicts with Octaryn architecture rules; deviation
recorded.

Code anchors: `octaryn-client/Source/Rendering/RenderBackend/` (DDGISystem,
DDGISchedule, DDGIOccupancy, DDGIDebug, LightingSystem) and
`octaryn-client/Shaders/DDGI/`, `Shaders/RayTracing/`, `Shaders/Voxel/`.

## Architecture and frame structure

| § | Plan item | Status | Evidence / notes |
| --- | --- | --- | --- |
| 1 | Overall architecture: mesher → BLAS, voxel data, event bus, scheduler | Partial | Streaming/edit events flow through `SceneChanges` into DDGI; no unified event bus, but equivalent invalidation path exists |
| 2 | One potential probe per voxel, derived position | Done | Fine cascade: spacing 1, cell-centered (`voxel + 0.5`), no stored positions; coarse volume remains for far field (user decision 2026-09-15) |
| 33 | One shared TLAS for shadows/reflections/DDGI | Done | `world_ray_bind` binds the same snapshot TLAS to all passes |
| 34 | Frame order: DDGI rays after current TLAS | Done | `RaySceneResource → ProbeResource` lighting graph edge; edits wait for `AccelerationReady` before non-opening invalidation |
| 35 | Async compute | Deferred | Plan says profile first; lighting profile timestamps exist to justify it later |
| 54 | Slang module layout | Partial | `Shaders/DDGI/` is split by pass (Types/Trace/Update/Seed/Sample/SkyVisibility/Environment/ProbeDebug); not as granular as proposed, no monolith either |
| 55 | Shared material evaluation across raster/RT/GI | Done | `world_ray_radiance`/`MaterialSampling`/`PbrEvaluate` shared by water, DDGI trace and debug paths |
| 56 | Ray-query-from-compute DDGI | Done | All DDGI/trace passes use inline `RayQuery` from compute on both backends |
| 57 | SER optional-only | Deferred | Not used; baseline is vendor-neutral as required |
| 58 | Pinned RHI behind Octaryn renderer API | Done | Standalone slang-rhi pinned, all access through `RenderBackend` wrappers |

## Probe lifecycle and scheduling

| § | Plan item | Status | Evidence / notes |
| --- | --- | --- | --- |
| 3 | Probe state machine (solid/dormant/wake/dirty/converging/active) | Partial | States exist: solid (`padding.x`), seed-only (`padding.y`), pending, active, sleeping (`offset.w`), changed (`refresh_frame`); no explicit converging/dormant variability states |
| 4 | Separate exists/useful/needs-work | Partial | Occupancy (Needed/Solid/Sky) + dirty flags approximate this; no dormant-with-history tier |
| 5, 20 | One-ray wake check for sleeping probes | Partial | Sleeping probes pay 0 rays for 120 frames, then revalidate through the normal path; no dedicated 1-ray classification probe |
| 6 | Cheap probe metadata | Done | `DDGIProbe` = 32 bytes: offset+state, version, last frame, stable/history counts |
| 7 | World/regional revisions instead of flag loops | Partial | Global scene/light revisions + per-column ring; no per-chunk GI revisions |
| 12 | Priority scheduler with buckets | Partial | CPU scored partial_sort (changed/age/proximity) + fresh reserve; no GPU compaction or bucket split |
| 13 | Variable ray counts per probe tier | Done (2026-09-15) | Selection packs a ray tier: burst = all 112 lighting rays, active = 48, background = 16; GPU culls the rest |
| 14 | Burst updates after major change | Done (2026-09-15) | Fresh/dirty probes get tier-0 burst with zeroed/reactive history; background tapers automatically |
| 24 | GPU work queue + indirect dispatch | Gap | CPU selection buffer, regular dispatch |
| 25, 26 | Per-chunk dirty bitmasks / GIChunk | Gap | Volume-level dirty flags instead of chunk bitsets |
| 36 | Variability EMA metric | Done (2026-09-15) | Per-probe variability buffer (EMA of max texel delta) gates sleep entry and drives the convergence debug view |
| 37 | Multi-sample stable counter before sleep | Done | 8 consecutive low-delta updates → sleeping; sleeping probes skip rays for 120 frames |
| 38 | Camera-distance priority | Done | Proximity term in the schedule score |
| 39 | Player-event (mining/explosion) priority | Partial | Opening voxel is seeded/ignored live and edits get the top changed score |
| 40 | GPU-time/ray budget, not fixed probe count | Partial | Fixed probe budget per frame (96 coarse / scaled fine) with tier-scaled ray counts; no time-based budgeting |
| 41 | Quality modes keep logical lattice | Done | Voxel GI radius option resizes the 1-block lattice without architecture change |
| 42 | Distance-based activation density | Partial | Coarse+fine volumes give far/near tiers; no mid-range activation thinning |

## Events and invalidation

| § | Plan item | Status | Evidence / notes |
| --- | --- | --- | --- |
| 8 | Block destruction pipeline (remesh → BLAS → TLAS → dirty AABB → wake) | Done | `SceneChanges` Added/Modified/Removed/AccelerationReady drive invalidation; BLAS rebuild (not refit) per edit |
| 9 | Chunk-boundary neighbor remeshing | Done | Halo/boundary rebuild path in streaming; DDGI uses world-space boxes as the plan allows |
| 10 | Dirty GI regions with reasons | Partial | Boxes with radius exist; no per-reason work splitting (irradiance vs visibility vs classify) |
| 11 | Tight rings, propagate via variability | Partial | Edit box + spacing×3 radius; no ring 1/2 staged priorities |
| 16 | Visibility/distance history updates with geometry | Done | Distance moments reset/blend on change; relocation recheck on reset only |
| 21 | Lights wake only their influence bounds | Done (2026-09-15) | Old+new light influence boxes invalidate; was full-volume wake |
| 22 | Quantized/budgeted sun updates | Done | Sun motion never invalidates probes; bounce updates via the continuous budget |
| 23 | Light importance threshold | Partial | Reach-bounded wake; no intensity/distance² cutoff below the range |
| 47 | Emissive voxels feed GI, light-dirty only | Partial | Emission flows through local lights into the DDGI trace; occupancy distinguishes occupancy vs light changes coarsely |
| 49 | Flicker smoothing (GI rate < visual rate) | Done (2026-09-15) | Light-count changes wake instantly; intensity-only revisions are rate-limited to every 10 frames |
| 51 | Per-event telemetry | Partial | 120-frame printf of scheduled/rays/invalidated + capture JSON; no per-event records |
| 52 | Event merging (explosion → one region) | Partial | Column-granular scene changes; each column box invalidates separately (radius-bounded) |
| 53 | Triple-buffered event/work buffers | Partial | Selection is double-buffered; controls upload on change only |
| 45 | Multiplayer: server replicates edits, client owns GI | Done | GI is fully client-side; only edits/light state cross the wire |

## Probes and geometry

| § | Plan item | Status | Evidence / notes |
| --- | --- | --- | --- |
| 15 | Dynamic hysteresis ramp after change | Done | Reset → 0 history, reactive 0.5 cap, cadence-scaled warmup to 0.94 |
| 17 | Relocation constrained to own cell | Done | ±0.45 spacing clamp; logical cell never changes |
| 18 | Voxel occupancy before ray relocation | Done | CPU occupancy classification; ray relocation only for partial geometry |
| 19 | No constant fixed classification rays | Done | Relocation/backface rays only run on reset or change |
| 27 | Metadata vs lighting payload split | Done | 32-byte probe struct separate from irradiance/distance atlases |
| 28 | Lower angular resolution for dense lattice | Done | 6×6 irradiance / 8×8 distance already; configurable by env |
| 29 | Surface sampling: 8-probe cage, validity, trilinear+normal+visibility weights | Done | `ddgi_sample_volume` implements the full gather |
| 30 | Never-black fallback chain | Done | Pending → coarser field → traced sky visibility → ambient floor |
| 31 | Reflections sample DDGI at hit | Done (2026-09-15) | Water reflection hits evaluate direct + DDGI bounce + emission |
| 32 | RT shadows stay separate from DDGI | Done | Independent passes and history |
| 43 | Underground/caves spend probes on geometry | Done | Transparency-aware occupancy: water/glass/leaves/sprites host probes, occluders displace them |
| 44 | Probe state persists with chunk streaming | Divergent | Camera-following volumes re-seed from neighbors instead; Octaryn persistence is edit-only, so probe payloads are never written to saves. The seed pass replaces this |
| 46 | Dynamic objects classified GI-insignificant | Done | Players/items are not in the voxel TLAS; separate player shadow AS |

## Quality, testing and tooling

| § | Plan item | Status | Evidence / notes |
| --- | --- | --- | --- |
| 48 | Multi-bounce with energy clamp | Partial | One bounce via `ddgi_sample` at ray hits + temporal feedback through hysteresis; no explicit second-bounce term |
| 50 | Debug views: state, rays heatmap, irradiance, distance, variability, dirty AABBs, BLAS rebuilds, wake flash, cost HUD | Partial | Probe spheres (irradiance/state/convergence incl. wake flash), dirty-region box overlay (view 27), surface debug 2–7, TLAS/BLAS views 9–11, lighting CSV profile. Missing: ray heatmap, on-screen cost HUD |
| 59 | Automated lighting scene tests | Partial | `ddgi_schedule_test.cpp` covers the scheduler; no scripted wall/door/tunnel scenes |

## Production phases (§60)

| Phase | Status | Notes |
| --- | --- | --- |
| 1 basic DDGI | Done | Lattice, RT rays, irradiance+distance, surface sampling |
| 2 voxel integration | Done | 1-block lattice, occupancy, block-event invalidation, transparency-aware |
| 3 persistent probe states | Done | Solid/seed-only/pending/active/sleeping with stable-counter sleep entry |
| 4 event-driven updates | Done | Edits, lights (influence-bounded, flicker-limited), emissives, dirty regions |
| 5 GPU scheduler | Gap | CPU scoring + upload; no GPU compaction/indirect dispatch |
| 6 adaptive rays | Partial | Tier bursts + sleep retry done; 1-ray wake classification missing |
| 7 polish (relocation/leak/hysteresis/multi-bounce) | Partial | Relocation, Chebyshev leak suppression, dynamic hysteresis done; multi-bounce minimal |
| 8 scale (streaming, persistence, distance activation, VRAM) | Partial | Streaming re-seed + radius options; probe persistence divergent by design |
| 9 platform tuning | Partial | Vulkan + DX12 qualified on Windows; Linux/macOS runtime unverified |
| 10 tools | Partial | Probe visualizer + profiling CSV; no ray heatmap, event visualizer, or scene tests |

## Next slices (priority order)

1. §5/20: 1-ray wake classification for sleeping probes near events.
2. §36: retain variability EMA in probe state for hysteresis/sleep decisions.
3. §50: ray-count heatmap view + on-screen GI cost HUD.
4. §24/25/12: GPU dirty-mask compaction, bucketed priorities, indirect dispatch.
5. §59: scripted wall/door/tunnel/explosion scene tests.
