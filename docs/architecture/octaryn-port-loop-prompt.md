# Octaryn Port Loop Prompt

Use this prompt when starting a new Octaryn port pass.

```text
Get to work in `/home/zacharyr/octaryn-workspace`.

Read first:
- `AGENTS.md`
- `REQUESTS.md`
- `docs/architecture/octaryn-cpp-engine-systems-finish-plan.md`
- `docs/architecture/octaryn-master-plan.md`
- `docs/architecture/octaryn-appendix.md`
- `DONE.MD`
- relevant `old-architecture/source/` files for the slice before editing

Current truth:
- The repo is not 100% finished until the finish plan completion definition is true.
- Inspection-only work is not completion.
- A clean build is not completion.
- A native bridge around a managed engine system is not completion.
- C# may remain only for shared/module API contracts, manifest/sandbox validation, module activation glue, and host bridge imports/exports.
- Client/server engine systems must be C++ owner code using existing Octaryn native libraries.
- `DONE.MD` is a status ledger, not a completion override. If it conflicts with the finish plan, the finish plan wins.

Priority order:
1. Preserve the fixed live client chunk-stream batching before touching lower-value cleanup.
2. Continue moving remaining client/server engine systems out of C# into focused C++ owner code.
3. Do opportunistic cleanup only around touched code.

Current guarded client behavior:
- `octaryn-client/Source/App/WorldMeshRuntime/WorldMeshRuntime.cpp` owns bounded per-frame server-stream mesh batches.
- `octaryn-client/Source/Rendering/EmptyWorldMesh/Geometry/TerrainMeshBatch.cpp` exposes selected-entry mesh construction.
- Radius-32 streaming must continue to grow through multiple bounded batches instead of one large build/upload frame.
- This older blocker is no longer the current truth: `WorldMeshRuntime` must not be described as still doing whole-stream synchronous server-stream mesh construction unless current source proves a regression.

Required guard shape:
1. Inspect current `WorldMeshRuntime`, `FrameLoop`, `TerrainMesh`, chunk mesh plan, world upload, native jobs runtime, and relevant old-architecture chunk/world job code.
2. Keep focused persistent server-stream mesh update state in client owner code.
3. Keep a focused `TerrainMesh` API that can append/build selected chunk/plan entries instead of the whole stream.
4. Step updates once per frame with a bounded chunk/column budget.
5. Keep `octaryn_native_jobs`/Taskflow on CPU build/packing work.
6. Keep GPU API calls and final upload application on the client main thread.
7. Preserve retained GPU resources, indirect draw, mipmaps, render-distance far plane, no LODs, and no full-world rebuilds for unchanged chunks.
8. Log enough runtime evidence to prove multiple bounded batches, reduced build/upload timing, stable indirect draw, retained chunks, and full radius-32 visibility.

Secondary priority:
Continue removing C# engine systems.

Current direction:
- Server still has too much managed engine-system code. Treat this as the current main blocker after guarding the client radius-32 path.
- Move persistence backends, player simulation, block storage, command queues, chunk streaming, terrain generation, world time, and hot-path storage into focused C++ owner code.
- Managed files may remain only as temporary interop glue until owner-correct C++ entry points replace them.
- Do not add new managed engine systems.

Server rules:
- Server is authoritative for edits, validation, simulation, saves, replication, and persistence.
- Seed terrain data stays memory/VRAM only.
- Only authoritative edited/different blocks and metadata may be persisted or streamed as block records.
- Avoid JSON churn and never write generated seed chunk data to disk.

Cleanup rules:
- Cleanup naming/folders around files you touch.
- Remove redundant owner prefixes where the path already provides ownership.
- Delete empty folders after moves.
- Keep all touched source/code files under 500 physical lines.
- Do not spend a whole pass on cosmetic cleanup while the chunk hitch or C# engine-system migration remains unfinished.

Validation rules:
- Do not use smoke tests unless explicitly requested.
- Do not run `ctest` unless explicitly requested.
- Use targeted builds/probes plus direct runtime/profiling evidence for performance work.
- For the client chunk batching fix, run the relevant targets that exist in the current tree, such as:
  - `tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_chunk_mesh_plan_probe`
  - `tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_empty_world_mesh_probe`
  - `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_jobs_probe`
  - `tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_app_launch_probe`
  - `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets`
  - `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_owner_boundaries`
- Then run a direct client/server runtime or launch probe long enough to verify radius growth and compare logs for:
  - radius reaches 32
  - multiple bounded `server_seed_memory` batches instead of one giant update
  - reduced `elapsed_ms`, `build_ms`, and `upload_ms`
  - stable `live_world_mesh_draw` indirect path
  - no recurring server JSON churn for unchanged windows

Work loop:
1. Inspect dirty files and do not revert unrelated changes.
2. Inspect relevant old-architecture source before behavioral changes.
3. Make a short source-to-destination ledger for the slice.
4. Implement the smallest owner-correct production fix.
5. Remove touched dead/duplicate/temporary code.
6. Validate with targeted builds and runtime/profiling evidence.
7. Update `DONE.MD` when a file or area becomes fully done for the finish-plan restriction, or when an existing done/not-done mark becomes stale.
8. Update finish-plan/docs only when status or scope changed.
9. Report exactly what changed, what was validated, and what remains.

Before finishing, confirm:
- `REQUESTS.md` was checked.
- The finish plan is still accurate or was updated.
- Client/server/shared/basegame ownership stayed clean.
- No generic `engine`, `runtime`, `common`, `helpers`, or temporary bucket was added.
- No touched source/code file exceeds 500 physical lines.
- No C# engine system was added.
- No seed terrain chunk data is persisted.
- No LOD workaround was introduced.
- Runtime/performance risk is explicitly reported if not validated.
```
