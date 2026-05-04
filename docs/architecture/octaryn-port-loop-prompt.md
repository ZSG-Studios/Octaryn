# Octaryn Port Loop Prompt

Use this prompt when starting a new Octaryn port pass.

```text
Get to work on the Octaryn port loop in `/home/zacharyr/octaryn-workspace`.

Use agents efficiently and effectively where parallel inspection or disjoint implementation helps. Follow `AGENTS.md`, the canonical master plan set rooted at `docs/architecture/octaryn-master-plan.md`, and the supplemental appendix/checklist in `docs/architecture/octaryn-appendix.md`.

Priority:
- `docs/architecture/octaryn-master-plan.md` and its focused sibling files are the source of truth for architecture, ECS/API direction, module policy, dependencies, validation gates, and phase order.
- `docs/architecture/octaryn-appendix.md` is supplemental migration/checklist material.
- If docs conflict, follow the master plan and call out the conflict. Do not silently invent a third direction.

Goal:
Port the old architecture into the new clean owner-split architecture until all useful old content, systems, runtime behavior, tools, shaders, validation paths, and performance-critical paths are accounted for. Do a full scan of what `old-architecture/` could do, follow the code, and port the behavior into the new architecture without missing systems.

Do not stop at placeholders, probes, static snapshots, or a window that merely opens. The new architecture must regain the old playable/rendered systems as real runtime features: camera controls, movement/input flow, chunk loading/streaming, chunk mesh generation, native chunk upload, sky rendering, atlas/material binding, normal/specular/PBR sampling, and visible world presentation.

The target is 100% useful parity with the old architecture, shaped correctly for:

- `octaryn-client/`
- `octaryn-server/`
- `octaryn-shared/`
- `octaryn-basegame/`
- `tools/`
- `cmake/`

Every old capability must end in one of these states:

- Implemented in the correct new owner.
- Replaced by a cleaner owner-correct implementation with equal or better behavior.
- Held as reference-only because the plan blocks it, such as DDGI/skylight before a dedicated lighting plan.
- Removed only with a clear documented reason.

Current failure to avoid:
- Do not treat `Octaryn.Client` opening a window and drawing static `SDL_Renderer` atlas tiles as a finished game client.
- Do not count validation-only `OCTARYN_CLIENT_APP_WORLD_BLOCKS_PATH` snapshots as real chunk loading.
- Do not count bundled shader files as PBR support until the active runtime binds and renders through the real shader/material pipeline.
- Do not count existing camera/math helper targets as camera gameplay until input, view movement, and presentation are wired into the active client loop.
- Do not call the port done when the screen has no sky, no chunk streaming, no camera motion, no real PBR path, or no interactive world.
- Do not treat a C# module scheduler as sufficient threading coverage for native terrain streaming. Chunk generation, chunk mesh build, stream parsing, upload preparation, and other heavy world/render data prep must be implemented in C++ owner code and use the owner-approved `octaryn_native_jobs` coordinator/worker-pool path with the approved Taskflow wrapper, or an explicitly documented owner-correct C++ replacement approved by the architecture plan.
- Do not hand-roll side schedulers, private worker pools, temporary thread loops, generic helper buckets, or duplicate replacements for existing Octaryn libraries. Use the existing Octaryn logging, diagnostics, profiling, memory, shader tooling, atlas, mesh packing, upload, validation, and dependency-wrapper libs correctly.
- Do not keep C# engine systems just because they already exist. C# in active client/server/shared code is limited to module API contracts, module activation/validation, host bridge exports/imports, and minimal bridge glue that cannot yet be owner-correctly expressed in C++. Engine systems must move to C++ owner code.
- Do not accept main-render-thread terrain mesh rebuilds, metadata stream parsing, full visible-world upload merges, or per-frame JSON churn as finished streaming.
- Do not accept unstable FPS during render-distance growth. Loading or expanding to 32 chunks must be bounded, batched, profiled, and proven with direct runtime evidence.
- Do not accept terrain that is visually one layer thick, has wrong backface/culling orientation, loses chunks below render distance, or requires LODs to hide bad streaming.
- Do not let seed terrain block data leave memory/VRAM as chunk JSON or disk save data. Only authoritative edits/differences may persist or stream as block records.

Hard rules:
- Inspect first, make a brief source-to-destination plan, then execute.
- Start each pass with current repo state, dirty files, `AGENTS.md`, the master plan, appendix, migration docs, validation docs, and relevant old source.
- Do not create any new `engine/`, `octaryn-engine/`, generic `runtime/`, `common`, `helpers`, `misc`, or catch-all buckets.
- Treat `old-architecture/` as source material only. New active code belongs in the correct new owner.
- Preserve behavior unless a boundary/API change is required.
- Keep client presentation/rendering/windowing/input/audio/UI out of server.
- Keep server authority/persistence/simulation/world saves/replication out of client.
- Keep gameplay/content/rules/items/blocks/recipes/tags/loot/product UI in basegame or another validated game module.
- Keep shared implementation-free and contract/API focused.
- Keep module/game/mod APIs explicit, capability-scoped, and deny-by-default.
- Keep build outputs under `build/<preset>/<owner>/` and logs under `logs/<owner>/`.
- Do not use smoke tests or `ctest`.

Full old-architecture scan:
Inventory the relevant old surface before choosing implementation work. Include, as applicable:

- App startup, window, display, fullscreen, audio, overlays, world-time presentation, and managed host bridge.
- Camera, player input, player state, spawn, movement, collision, selection, raycast, and interaction UX.
- Block definitions, classification, water/lava/static material behavior, hidden blocks, support/replacement rules, content catalogs, and basegame data.
- World generation, terrain, features, noise, chunk lifecycle, chunk windowing, runtime queries, snapshots, edit queues, and surface queries.
- Chunk generation, chunk meshing, direct upload, upload telemetry, render descriptors, visibility, packed mesh paths, and streaming behavior.
- Atlas, materials, PBR/normal/specular sampling, render pipelines, scene passes, postprocess, resources, UI rendering, shader metadata, shader tools, and all shader sources.
- Persistence, settings, world saves, chunk files, cache, player state, world time, serialization, compatibility IDs, corruption handling, and migration replay.
- Runtime jobs, world jobs, Taskflow scheduling, worker policy, worker pool, barriers, and profiling ownership.
- Jolt physics service, worker jobs, layers, tracing, collision, and prediction/authority boundaries.
- Logging, checks, crash diagnostics, env setup, memory/mimalloc, profiling hooks, and owner logs.
- Old CMake, old build helpers, packaging scripts, validation docs, launch probes, and tool entry points.
- Lighting, skylight, and DDGI paths only as reference material unless the user has provided an explicit lighting plan.

Required playable-client parity slices:
- Client runtime loop: replace probe-only presentation with a real client runtime path that owns window events, input state, frame pacing, camera/view state, and presentation handoff.
- Camera/input: port old camera/player input behavior into `octaryn-client` presentation and basegame/server authority boundaries as required; prove keyboard/mouse movement works on screen.
- Chunk loading and streaming: port old chunk windowing, generation handoff, lifecycle, streaming, dirty/update queues, and snapshot ingestion into server/client owners; validation-only static files are not enough.
- Chunk mesh and upload: port old mesh build, packed mesh paths, visibility, render descriptors, direct upload, telemetry, and GPU upload behavior into owner-correct native paths.
- Jobs/threading: port or rebuild the old native jobs/world-jobs path in C++ behind the new owner contract, using `octaryn_native_jobs` and the approved Taskflow wrapper rather than C# scheduler workarounds or private thread pools. The main client thread may poll results and issue graphics API work, but it must not generate terrain, mesh full chunk rings, parse large stream payloads, or merge/upload thousands of chunk records synchronously during the frame.
- Sky and scene rendering: bind the old sky, opaque, transparent, water, sprite, depth, selection, clouds, postprocess, and composite shader paths through the active client renderer, not only bundle them as files.
- PBR/material path: bind color, normal, specular, animation, LabPBR/POM-related sampling, mip behavior, and material atlas data in the active renderer; static SDL texture swatches do not count.
- Interaction: port raycast selection, block break/place command flow, accepted/rejected edits, support/replacement rules, and server snapshot replication into the visible client loop.
- Runtime validation: run the actual visible client where practical and verify camera movement, sky, streamed chunks, PBR/material rendering, and FPS/chunking behavior from logs/profiling/captures.

Performance rules:
- Keep blazing-fast chunking and FPS as first-class acceptance criteria.
- Preserve or improve fast chunk generation, chunk meshing, upload, streaming/window updates, render descriptors, visibility, and render hot paths.
- Do not replace fast native hot paths with slower managed-only or allocation-heavy code unless profiling proves the replacement is equal or better.
- Keep high-throughput chunk/world/render/simulation work on owner-owned native or scheduled hot paths where needed.
- Use bounded incremental streaming and upload batches. Expanding render distance must not clear/rebuild the whole world, upload unchanged chunks, or block the render frame on large CPU mesh work.
- Keep indirect rendering active for the terrain/world hot path. If a pass cannot use indirect draw, document why and profile it against the old path.
- Keep mipmaps active for atlas/material textures, while UI/composite/present paths use the correct non-mip sampler where needed.
- Far-plane behavior must be derived from render distance and camera needs, not an arbitrary clip that hides valid chunks.
- No LODs are allowed for hiding chunk streaming or mesh cost unless the user explicitly requests LODs.
- Use direct runtime runs, targeted builds, owner launch probes, focused profiling logs, Tracy captures, RenderDoc captures where useful, and targeted benchmarks for validation.
- Report any FPS/chunking risk explicitly if it was not validated.

Current regression loop:
1. Inspect the latest dirty client/server streaming, terrain, mesh upload, render, scheduler, persistence, and validation changes before editing.
2. Inspect old-architecture `runtime/jobs/`, `world/jobs/`, `world/chunks/`, render world upload/draw code, atlas mip upload code, render-distance code, and stream benchmark/debug code.
3. Build a source-to-destination ledger specifically for: native jobs worker pool, world job scheduler, chunk window lifecycle, dirty chunk queues, mesh build jobs, upload staging, direct/indirect draw, frustum culling, atlas mipmaps, far plane/render distance, terrain face orientation, server edit authority, and edit-only persistence.
4. Fix the threading architecture first in C++: heavy chunk stream parsing, seed terrain mesh construction, meshing, and upload-prep must be scheduled off the render frame through `octaryn_native_jobs`/Taskflow and the approved owner job path. Main thread only consumes ready bounded batches and performs graphics API calls.
5. Fix chunk streaming next: render distance 32 must request, receive, mesh, upload, and draw the whole visible area in bounded time without JSON seed-block payloads, unnecessary full rewrites, or full-world rebuilds.
6. Fix terrain correctness: no one-layer terrain illusion, no wrong side/backface culling, no missing chunk rings, no hidden seed terrain below the visible surface, no LOD workaround.
7. Fix render hot paths: retained per-chunk GPU buffers, indirect draw, frustum culling, correct atlas/material samplers, active mip chains, no per-frame target churn, no per-frame full buffer rebuild.
8. Profile after every coherent change with the real bundled client and server. Compare normal frame cost, radius-growth spikes, `world_ms`, `sim_ms`, opaque draw cost, uploaded chunk count, retained chunk count, and 32-chunk load time.
9. Do not stop until runtime evidence shows stable FPS during streaming and the remaining blockers are either fixed or precisely listed with file-level causes.

Work loop:
1. Inspect dirty files and avoid reverting unrelated user changes.
2. Inspect current docs and old source for the scope.
3. Build a source-to-destination ledger for all old files/capabilities discovered in scope.
4. Assign each item to client, server, shared, basegame, tools, cmake, reference-only lighting hold, or documented removal.
5. Identify the highest-value non-blocked slice. Prefer real playable-client blockers first when the current visible runtime lacks camera, chunks, sky, PBR/material rendering, or interaction.
6. Keep pure moves, ownership/API changes, and behavior changes reviewable.
7. Remove dead, duplicate, temporary, compatibility, or legacy code touched by the task.
8. Validate with targeted owner checks and runtime/profiling evidence where practical.
9. Update docs only when the task requires it or when discovered scope must be recorded for future port work.
10. Report exactly what changed, what old behavior it covers, what was validated, and what remains open.

Before finishing, confirm:
- Maximum useful agents/subagents were used where applicable.
- Client/server/shared/basegame boundaries stayed clean.
- No generic engine/runtime bucket was added.
- Naming is simple and consistent.
- Comments are minimal and useful.
- No unapproved dependencies or module-facing internals were introduced.
- Old files and capabilities in scope were mapped to explicit destination owners or documented removal reasons.
- Chunking, meshing, upload, streaming, and FPS-sensitive paths were preserved or improved, or any remaining risk was clearly called out.
- The visible runtime was not accepted as complete if it only opened a window, drew static atlas tiles, or used validation-only snapshots.
- Camera movement, chunk loading/streaming, sky rendering, and PBR/material rendering were either implemented and validated or listed as explicit remaining blockers.
- No old content, system, shader, tool, validation path, or runtime behavior in scope was silently skipped.
- Builds, targeted checks, runtime runs, profiling captures, or structure checks were executed where practical, or the reason they were not run is clear.
```
