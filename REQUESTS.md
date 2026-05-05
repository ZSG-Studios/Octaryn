# Request Buffer

`REQUESTS.md` is a repo-local buffer for extra prompts, standing requests,
cleanup directives, and temporary user instructions that should be visible to
future agents.

Agents must read this file before starting repo work. Treat entries here as
additional user instructions layered on top of `AGENTS.md`.

## Entry Types

Use one of these labels at the start of each request entry:

- `LOOP:` a standing request. Keep it in this file after acting on it.
- `ONE_TIME:` a single-use request. Remove it only after the requested work is
  fully completed and verified.

## Removal Rules

Agents may remove:

- Completed `ONE_TIME:` entries after the work is done and the final response
  reports that the entry was completed and removed.
- Duplicate entries only when the duplicate says the same thing and removing it
  does not weaken the request.
- Entries explicitly removed by the user.

Agents must not remove:

- Any `LOOP:` entry.
- Any unclear, partially completed, blocked, or unverified `ONE_TIME:` entry.
- Any request that defines architecture, ownership, naming, validation, cleanup,
  or safety policy.
- Any request that another active task still depends on.

## Editing Rules

- Do not rewrite user intent into softer language.
- Do not merge unrelated requests into one entry.
- Keep entries short, direct, and actionable.
- If a request needs clarification, leave it in place and note the blocker in
  the final response.
- If a `ONE_TIME:` entry is completed, remove only that completed entry and
  leave the rest of the file intact.

## Active Requests

- LOOP: Guard the client 32-chunk streaming hitch fix. Do not regress
  `WorldMeshRuntime` to building one full radius-32 stream and uploading it
  synchronously in one frame. Keep bounded per-frame server-stream mesh
  batching, the focused `TerrainMesh` selected chunk/plan-entry API, GPU calls
  on the client main thread, `octaryn_native_jobs`/Taskflow for heavy work, and
  runtime proof with multiple bounded `server_seed_memory` batches, stable
  indirect draw, reduced build/upload timing, and full radius-32 visibility.
- LOOP: Continue removing C# engine systems. C# may remain only for
  shared/module API contracts, manifest and sandbox validation, module
  activation glue, and host bridge imports/exports. Client/server engine systems
  such as terrain generation, chunk streaming, chunk meshing, render/upload data
  preparation, persistence backends, player simulation, world time, command
  queues, scheduling execution, and hot-path storage must move to focused C++
  owner code using existing Octaryn native libraries. Do not add new managed
  engine systems.
- LOOP: Preserve server authority and edit-only persistence. Server owns
  validation, simulation, edits, saves, replication, and persistence. Seed
  terrain data must stay memory/VRAM only; only authoritative edited/different
  blocks and metadata may be streamed as block records or written to disk. Avoid
  JSON churn and never persist generated seed chunk data.
- LOOP: Keep cleanup/naming work opportunistic during real fixes. Remove
  redundant owner prefixes and empty implementation-language buckets when
  touching nearby files, but do not spend a pass on cosmetic cleanup while the
  client streaming hitch, native job path, or C# engine-system migration remains
  unfinished. Keep all touched files focused, owner-correct, and under 500
  physical lines.
