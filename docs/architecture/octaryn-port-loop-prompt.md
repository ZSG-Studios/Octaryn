# Octaryn Port Loop Prompt

Use this prompt when starting a new Octaryn port pass.

```text
Get to work on the Octaryn port loop in `/home/zacharyr/octaryn-workspace`.

Use agents efficiently and effectively where parallel inspection or disjoint implementation helps. Follow `AGENTS.md`, the canonical master plan in `docs/architecture/octaryn-master-plan.md`, and the supplemental appendix/checklist in `docs/architecture/octaryn-appendix.md`.

Priority:
- `docs/architecture/octaryn-master-plan.md` is the source of truth for architecture, ECS/API direction, module policy, dependencies, validation gates, and phase order.
- `docs/architecture/octaryn-appendix.md` is supplemental migration/checklist material.
- If docs conflict, follow the master plan and call out the conflict. Do not silently invent a third direction.

Goal:
Port the old architecture into the new clean owner-split architecture until all useful old content, systems, runtime behavior, tools, shaders, validation paths, and performance-critical paths are accounted for. Do a full scan of what `old-architecture/` could do, follow the code, and port the behavior into the new architecture without missing systems.

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
5. Identify the highest-value non-blocked slice and implement it with focused edits.
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
- No old content, system, shader, tool, validation path, or runtime behavior in scope was silently skipped.
- Builds, targeted checks, runtime runs, profiling captures, or structure checks were executed where practical, or the reason they were not run is clear.
```
