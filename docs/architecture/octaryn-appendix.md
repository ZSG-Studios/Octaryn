# Octaryn Appendix

> **For agentic workers:** Use the maximum available agents/subagents where available. Inspect first, make a brief source-to-destination plan, then execute. This file defines structure and boundaries only.

`docs/architecture/octaryn-master-plan.md` is the canonical Octaryn master plan. This file is an appendix and migration checklist. If this file conflicts with the master plan, update this file to match the master plan.

**Goal:** Same end goal: port Octaryn into strict client, server, basegame, shared, and focused native support layers with a native C/C++ core first, no general `engine` folder, and no behavior rewrite. C# ECS/gameplay and client/server networking are intentional tools where they fit best. The platform should load game modules through explicit APIs and reject incompatible modules before they run.

**Architecture:** `old-architecture/` is source material only, never an active implementation target. Client owns presentation and rendering, server owns authority and persistence, basegame is the first bundled game module with high-level mechanics, content, and assets through API contracts, and shared owns contracts, value types, module manifests, compatibility rules, and validation-facing APIs. Existing focused support libraries should stay as build/internal libs under the owner that uses them instead of becoming a new generic runtime root.

**Tech Stack:** C++23/C17, CMake, SDL3 GPU/Vulkan, .NET 10, C# latest, Arch ECS for managed gameplay/module ECS, native owner ECS/storage for high-throughput host paths, LiteNetLib and LiteEntitySystem as hidden client/server networking backends behind Octaryn contracts, Taskflow under Octaryn-owned scheduling policy, Tracy, targeted runtime/profiling validation. RenderDoc is an external developer tool, not a workspace-managed dependency.

---

## Current State

- The active repository root is `/home/zacharyr/octaryn-workspace`.
- The old `octaryn-engine/` tree is deleted from the working tree and preserved as tracked `old-architecture/` source material.
- `octaryn-client/`, `octaryn-server/`, `octaryn-shared/`, and `octaryn-basegame/` have real owner project files.
- `octaryn-client/` owns the managed native ABI export edge through `Source/HostBridge/` and a client-owned `BasegameModuleActivator`; `octaryn-server/` owns the server host export edge, server module activation, and server-side module validation.
- `octaryn-basegame/` contains the current managed game context and basegame module registration. The old gameplay migration map now lives under `docs/migration/` so active basegame source stays focused on content and gameplay implementation.
- `octaryn-shared/` now contains timing/input-only host-frame contracts, a narrow request command contract, module manifests, dependency/content/asset/compatibility declaration records, exposed host API IDs, module capability IDs, module runtime/build package allowlists, framework API group allowlists, sandbox denied-group IDs, and manifest validation for duplicate, blank, unexposed API, unapproved capability, unapproved package, unapproved framework API group, and malformed declaration requests.
- Root MSBuild policy rejects unknown project owners, package references in `octaryn-shared`, host-only packages outside client/server, unapproved direct module packages, analyzer packages with runtime assets, unapproved resolved runtime/analyzer packages for module owners, and unclassified packages in module `project.assets.json`.
- Active `cmake/` has a concrete new-architecture scaffold: root `CMakeLists.txt`, root `CMakePresets.json`, owner CMake modules, dependency policy placeholders, platform modules, toolchain files, and new `tools/build` wrappers. It builds managed owner targets, native owner aggregates, hostfxr bridge facades, launch probes, bundles, debug tool payloads, and validation targets without porting the old monolith.
- Root debug tooling is first-class under `tools/`: `tools/ui/` owns the PySide workspace control app, `tools/profiling/` owns Tracy build/launch/capture, and `tools/build/` owns build orchestration, Linux host setup, Podman builder files, bootstrap entrypoints, sysroot setup, and shared shell helpers.
- `docs/` is the top-level informational/documentation-only folder. It is not a source, build, module, or runtime owner.
- `old-architecture/.octaryn-cache/` may contain ignored generated/reference cache files. Do not treat cache content as tracked source material or a migration source unless it is explicitly promoted.
- `old-architecture/tools/build/layout.sh` still points native builds at `old-architecture/`; active root `tools/build/` is reserved for intentionally ported new-architecture build helpers.
- Reference material lives under `refrances/`, including Minecraft 26.1.2, Iris, and Complementary Reimagined.
- DDGI, skylight propagation, lighting architecture, and the old CPU skylight implementation are on hold until the user provides a dedicated lighting plan. The current basegame `skylightOpacity` catalog value is content metadata only; do not add server lighting contracts, DDGI code, lighting probes, or client lighting rewrites before that plan exists.
- The next active port slice should stay non-lighting: continue basegame content/rules, server authority/persistence, client presentation that does not alter lighting, module validation, build ownership, or tool cleanup.
- The ECS/API direction is captured in `docs/architecture/octaryn-master-plan.md` and its focused sibling files: blocks, items, entities, UI state, input actions, game state, fluids, gases, and world interactions are ECS-backed declarations and systems; C++ hosts own fast backend execution, networking, persistence, scheduling, and validation behind explicit APIs.
- The dependency and subsystem direction from `/home/zacharyr/Downloads/deep-research-report.md` is finalized as planning guidance with project corrections that Arch ECS, LiteNetLib, and LiteEntitySystem remain in use: Arch ECS for managed gameplay/module ECS, native owner ECS/storage for high-throughput host paths, Jolt-first hidden physics, custom retained UI with hidden Yoga layout, LiteNetLib/LiteEntitySystem-backed networking behind Octaryn contracts, custom binary saves, Glaze JSON metadata, LZ4 hot compression, Zstd cold compression, and no public all-in-one runtime stack.
- The core host baseline is a flying camera with no default player physics and flat blank terrain. Target worlds are 512 blocks tall and centered vertically; existing 256-height or chunk-edge-derived height constants are migration debt, not the destination model.
- Singleplayer must still run through server authority. `client_server_app` must carry the bundled `server/` payload, start that server when creating or loading a singleplayer world, wait for world setup/module validation/save initialization to complete, then connect through the same command/snapshot contracts used by multiplayer. The standalone server bundle remains a dedicated headless terminal/server executable path.
- Product UI, including the main menu, pause menu, inventory screens, HUD, world-space panels, nameplates, block/entity panels, and game-specific options, belongs to `octaryn-basegame` or another active game module through explicit UI contribution APIs. Core/client-owned UI is limited to debug, diagnostics, profiler, validation, editor/developer, and emergency host surfaces. The client renderer owns screen-space rendering, render-to-texture, textured world-space quads/panels, focus, and raycast input routing.
- Developer-facing math, geometry, deterministic random, time, diagnostics, serialization, networking, and physics tools may be exposed only as Octaryn-owned API contracts and capability-gated handles. Do not expose raw physics worlds, transport sessions, renderer handles, native pointers, third-party backend types, sockets, filesystems, schedulers, or raw ECS storage to basegame, game modules, or mods.

## Phase 0 Blockers

These are current transitional violations and hard blockers. Do not add or expand module-facing behavior that depends on them. Work touching these areas must remove the blocker, add real enforcement, or keep the affected code non-activated.

- Keep `octaryn-basegame` on `octaryn-shared` contracts and do not reintroduce a reference to `old-architecture/source/api/Octaryn.Engine.Api.csproj`.
- Keep unmanaged managed-host exports in host-owned code such as `octaryn-client`, not `octaryn-basegame`.
- Keep `AllowUnsafeBlocks` out of `octaryn-basegame`. Module code must not keep unsafe/native bridge access as a normal permission.
- Keep unsafe native function-pointer bridges out of `octaryn-shared`; shared exposes safe module contracts such as manifests, module frame contexts, module command request facades, declarations, and capability handles. Raw host frame/command ABI types are owner/internal only.
- Keep host-only package references out of `octaryn-basegame`; `LiteNetLib` and `LiteEntitySystem` belong only in client/server transport projects when transport is wired.
- Keep LiteEntitySystem host-owned and hidden behind Octaryn networking contracts. It is part of the intended client/server networking stack, but not a module-facing API.
- Keep `Octaryn.Client.csproj`, `Octaryn.Server.csproj`, and `Octaryn.Shared.csproj` as real SDK project definitions with owner-routed outputs.
- Keep pre-load manifest validation file-backed: module content declarations must point at existing `Data/` records, asset declarations must point at existing `Assets/` or `Shaders/` files, and undeclared content/assets must fail validation.
- Replace runtime `legacy*` content schema fields with stable Octaryn IDs or generator-only migration metadata before treating basegame catalogs as final module data.
- Keep resolved transitive package validation enforced for basegame and extend the same runtime/build-analyzer allowlist model to external game modules and mods when those package projects are introduced.
- Keep source-level framework API allowlist enforcement and post-build binary metadata inspection active for namespaces/types/members. External binary-only modules still need artifact identity/package/content binding before they are trusted.
- Keep the owner thread contract enforced before porting heavy compute systems: one main thread, one coordinator thread, and a scalable worker pool with at least two workers. All computation and gameplay logic must be scheduled through this pool or through host APIs backed by this pool, with runtime/profiling validation expanded before heavy systems move.
- Keep owner-project validation that rejects host-only packages outside `octaryn-client` and `octaryn-server`.
- Expand the new CMake scaffold with native owner targets and targeted platform configure checks before claiming native platform/toolchain parity with the old monolith.
- C/C++ owner code may call managed host exports only through the resolved hostfxr owner bridge. Bridge readiness requires exact managed method-name resolution, ABI size/version validation, owner bundle discovery, failure-path validation, and direct runtime launch evidence.

## Hard Boundaries

- No new top-level `engine/` or `octaryn-engine/` folder.
- No new `Octaryn.Engine.*` namespaces.
- Do not port by copying the monolith shape into a new name.
- Do not rewrite behavior first; preserve behavior by moving it into the right owner.
- Do not make root `cmake/` a dumping ground. Shared build policy, owner targets, dependencies, platform detection, and toolchains must stay in separate named folders.
- Do not mix host platform logic with owner target definitions. Windows and Linux distro-family platform logic must be isolated behind platform/toolchain modules.
- Do not put networking packages in `octaryn-basegame`.
- Do not put GPU upload, mesh upload, render descriptors, windowing, audio, or UI in server.
- Do not put authoritative world edits, save ownership, or server simulation in client.
- Do not implement singleplayer as client-local authority. Singleplayer is `client_server_app` with client presentation connected to a bundled server-owned simulation, persistence, validation, replication, and physics path.
- Do not put product-specific game rules in shared or native support libraries.
- Do not put voxel host internals in `octaryn-basegame`: chunks, mesh data, lighting propagation, persistence, replication, transport, storage formats, or low-level world mutation belong to shared/client/server APIs and implementations.
- Do not load a game module by reaching into its internals. Game modules must declare their API version, required capabilities, content registrations, assets, dependencies, and compatibility constraints through shared contracts.
- Do not let client, server, or tools silently accept incompatible game modules. The host must validate manifests, API versions, dependency ranges, required capabilities, content IDs, asset declarations, and multiplayer compatibility before activation.
- Do not expose broad implementation assemblies to game modules. Module code may use only explicitly approved shared APIs, approved host interfaces, and approved .NET packages.
- Do not allow module-owned NuGet dependency drift. Game modules must compile against a deny-by-default package allowlist; unapproved .NET packages, framework namespaces, reflection, native interop, filesystem, process, threading, dynamic loading, and networking access are rejected unless the allowlist says otherwise.
- Do not let gameplay, module, client, server, or tool code create arbitrary computation threads. The host owns threading; systems submit work to the coordinator and worker pool through approved scheduling APIs.

## Destination Roots

```text
octaryn-client/
octaryn-server/
octaryn-basegame/
octaryn-shared/
tools/
cmake/
  Shared/
  Owners/
  Dependencies/
  Platforms/
    Windows/
    Linux/
  Toolchains/
    Windows/
    Linux/
docs/
refrances/
old-architecture/
build/<preset>/<owner>/
build/<preset>/deps/
build/dependencies/
logs/<owner>/
```

## Threading And Work Scheduling

The active architecture must use one explicit host-owned scheduling model. All systems that do meaningful computation or gameplay logic must be written so they can run safely through that model.

Thread roles:

- Main thread: owns process startup/shutdown, platform event pumping, presentation handoff, final frame submission, and the narrow places where a platform or graphics API requires main-thread access. It must not become the place where gameplay, chunk generation, simulation, asset processing, or other bulk computation runs.
- Coordinator thread: owns frame/tick scheduling, dependency graph assembly, work submission, synchronization fences, cancellation, and deterministic handoff between client, server, basegame/module logic, and tools. It schedules work; it does not run bulk work itself except for tiny coordination tasks.
- Worker pool: owns computation. It starts with a minimum of two worker threads and scales up to the available system cores according to host policy. The pool is the execution path for simulation systems, gameplay systems, world generation, mesh/data preparation, asset processing, validation jobs, async save/load preparation, replication preparation, and other CPU-heavy logic.

Scheduling rules:

- All computation systems and gameplay logic must run as jobs in the worker pool or through approved host APIs that schedule onto the pool.
- New code must be thread-safe by default: no hidden global mutable state, no unsynchronized shared containers, no lifetime borrowing across jobs without an explicit owner, and no blocking waits on the main thread.
- Systems must declare their read/write access, ordering dependencies, cancellation behavior, and frame/tick ownership before they enter the scheduler.
- Client presentation work may prepare data on workers, but graphics API calls, window events, and final presentation stay on client-owned main-thread/platform paths.
- Server authority work runs through coordinator-scheduled jobs and commits through deterministic server tick barriers. Save, validation, replication, and world-edit commits must have explicit synchronization points.
- Basegame and external modules do not own threads, tasks, or timers. They receive scheduled system/update entry points and capability handles from the host.
- Tools may use the same scheduler model for offline work, but tool-specific worker use must still be isolated under `tools/` or the owning module tool folder.

API and sandbox impact:

- `octaryn-shared` should expose scheduling contracts only as stable capability-shaped APIs: job scopes, tick phases, read/write declarations, cancellation tokens, and result handles. It must not expose native worker internals or third-party scheduler types.
- Client/server own scheduler implementations and thread creation. Shared/basegame/modules own only contracts and scheduled logic.
- Approved packages such as `Arch.System` may define gameplay systems, but those systems must be driven by host scheduling rather than direct task/thread creation from module code.
- Native C/C++ owner code may drive managed ECS/gameplay or networking through explicit owner bridges. Those bridges are host implementation details, not module APIs.
- Raw `System.Threading`, `Task.Run`, custom timers, unmanaged threads, and ad hoc thread pools remain denied for module code. Any exception requires a documented host API and an allowlist update.
- Native job support should use the approved `octaryn::deps::taskflow` wrapper through focused owner targets such as `octaryn_native_jobs`; do not hand-roll a parallel scheduler in client, server, or basegame.
- Taskflow executes Octaryn schedules; it does not define module scheduling policy. Octaryn owns phases, read/write declarations, barriers, cancellation, deterministic ordering, and profiler ownership.

Validation requirements:

- Source/API validation must reject raw threading and task scheduling from module code.
- Module manifests must declare scheduled systems with phase, owner, resource reads/writes, ordering, flags, and commit barrier before a host can activate scheduled work.
- Scheduler-facing systems must have targeted runtime/profiling validation through direct runs, Tracy captures, focused logs, or benchmarks. Do not use smoke tests or `ctest` as a substitute unless explicitly requested.
- CMake and MSBuild owner targets must keep scheduler support owner-partitioned under `build/<preset>/<owner>/` and `logs/<owner>/`.

## Client And Server Launch Modes

Octaryn has two user-facing launch modes that share the same authority model.

- Graphical client: `octaryn_client_bundle` is the local playable application. It owns windowing, input, rendering, audio, client UI, local prediction, presentation, and the user flow for choosing singleplayer or multiplayer.
- Client server app: when the client creates or loads a singleplayer world, `client_server_app` starts a server-owned local session from the bundled `server/` payload. That session owns world creation/loading, module validation, basegame activation, server ticks, simulation, persistence, replication state, physics, and shutdown.
- Dedicated server: `octaryn_server_bundle` remains separately runnable as a headless terminal/server package. It owns the same authority path as the bundled server, but has no client rendering, audio, UI, windowing, or GPU dependencies.
- Multiplayer client: remote multiplayer uses the same shared command, snapshot, tick, replication, disconnect, and error contracts as singleplayer. Singleplayer must not use privileged client-only mutation paths.

Packaging and ownership rules:

- The client bundle may copy server-owned artifacts into `server/` for singleplayer, but copied artifacts do not change ownership. Server implementation remains in `octaryn-server/`, server build outputs remain under `build/<preset>/server/`, and client build outputs remain under `build/<preset>/client/`.
- Do not create a monolithic target that compiles client presentation and server authority into one owner. Bundle composition is allowed; ownership mixing is not.
- The bundled `server/` payload must be version-matched to the client, shared contracts, and selected game/basegame modules before activation.
- Basegame and modules are validated before the bundled server reports readiness. The client may show progress and errors, but validation and activation decisions stay server/shared-contract driven.
- Bundled server logs stay under `logs/server/`; graphical client logs stay under `logs/client/`, even when both are launched from the client.

Startup readiness contract:

1. Client selects create/load singleplayer world and requested game modules.
2. Client starts or attaches to the bundled server session through a client-owned launch/supervision API.
3. Server validates module manifests, compatibility, requested capabilities, content/assets, package allowlists, and multiplayer/singleplayer compatibility.
4. Server creates or opens the world save, initializes server world state, activates basegame/modules, and starts server ticks.
5. Server publishes a local endpoint/session handle and a ready snapshot.
6. Client connects through shared command/snapshot contracts, receives initial state, and enters world presentation.
7. Client return-to-menu, quit, crash, or world-close flows stop the bundled server through server-owned shutdown APIs and wait for save-close completion where required.

Planned validation:

- Keep dedicated server launch probes separate from `client_server_app` launch probes.
- Keep the `client_server_app` readiness probe focused on the bundled server app startup path and expand it toward ready snapshot publication, clean disconnect, and save-close behavior as those runtime contracts land.
- Add bundle validation that confirms the client bundle contains the required `server/` payload and that the dedicated server bundle contains no client presentation payload.

## Non-Lighting Port Queue

Use this queue when choosing the next old-architecture slice. The master plan remains authoritative; this list is a source-to-destination checklist for incomplete non-lighting behavior.

| Priority | Slice | Old source | Destination owners | Current status |
| --- | --- | --- | --- | --- |
| 1 | Client bundle data and asset discovery | `old-architecture/source/core/asset_paths.*` and app startup reads | `octaryn-client/Source/Native/AssetPaths/`, client launch probes | Client launch validation reads module `Data/` and atlas `Assets/` from the bundled client payload without workspace fallbacks. |
| 2 | Native atlas upload and material rendering | `old-architecture/source/render/atlas/`, `old-architecture/source/render/world/`, atlas shaders | client rendering, shaders, asset tooling | Graphical client validation samples bundled color-atlas tiles and validation-only normal/specular material swatches through SDL texture rendering; full SDL_GPU atlas-array upload, shader binding, mips, animation updates, and PBR validation remain open. |
| 3 | Player movement, collision, spawn, camera state | `old-architecture/source/app/player/`, `old-architecture/source/core/camera/camera.*`, `old-architecture/source/physics/` | basegame movement rules, server physics, client prediction/presentation | Block selection rules are probed; authoritative movement/collision and spawn alignment remain unported. |
| 4 | Block raycast and interaction UX | `old-architecture/source/app/overlay/interaction.*`, `old-architecture/source/world/edit/` | shared commands/queries, basegame rules, server edits, client input | Server block edits and basegame support/replacement rules are probed; client raycast target selection and UX flow remain incomplete. |
| 5 | World save metadata, player save, and chunk override format | `old-architecture/source/core/persistence/` | server persistence with shared compatibility contracts | World block overrides, time, player save file round trips, current JSON chunk-column override files, and active world-save metadata summaries are present; compressed chunk cache semantics and migration replay remain open. |
| 6 | Dynamic fluids and gases | `old-architecture/source/world/edit/water.cpp`, old block/material behavior, and future fluid declarations | basegame declarations, server active-region simulation, client snapshots | Static water/lava catalog data exists; dynamic fluid/gas simulation and budget validation remain open. |
| 7 | Items, inventories, recipes, tags, loot, and product UI | old gameplay/content/UI source | basegame content/gameplay/UI, client retained UI execution | Hand item and basic interaction data exist; full inventory, recipes, tags, loot, HUD/menu/UI contribution flow remain open. |


## Detailed Appendices

- Owner roots and port-source maps: `docs/architecture/octaryn-appendix-owner-roots.md`.
- Module loading, compatibility, and sandbox policy: `docs/architecture/octaryn-appendix-module-policy.md`.
- Support libraries, CMake/build policy, target names, validation, and phase order: `docs/architecture/octaryn-appendix-build-and-libraries.md`.
