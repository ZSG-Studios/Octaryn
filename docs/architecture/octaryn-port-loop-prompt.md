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
- Sky and scene rendering: bind the old sky, opaque, transparent, water, sprite, depth, selection, clouds, postprocess, and composite shader paths through the active client renderer, not only bundle them as files.
- PBR/material path: bind color, normal, specular, animation, LabPBR/POM-related sampling, mip behavior, and material atlas data in the active renderer; static SDL texture swatches do not count.
- Interaction: port raycast selection, block break/place command flow, accepted/rejected edits, support/replacement rules, and server snapshot replication into the visible client loop.
- Runtime validation: run the actual visible client where practical and verify camera movement, sky, streamed chunks, PBR/material rendering, and FPS/chunking behavior from logs/profiling/captures.

Performance rules:
- Keep blazing-fast chunking and FPS as first-class acceptance criteria.
- Preserve or improve fast chunk generation, chunk meshing, upload, streaming/window updates, render descriptors, visibility, and render hot paths.
- Do not replace fast native hot paths with slower managed-only or allocation-heavy code unless profiling proves the replacement is equal or better.
- Keep high-throughput chunk/world/render/simulation work on owner-owned native or scheduled hot paths where needed.
- Use direct runtime runs, targeted builds, owner launch probes, focused profiling logs, Tracy captures, RenderDoc captures where useful, and targeted benchmarks for validation.
- Report any FPS/chunking risk explicitly if it was not validated.

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
