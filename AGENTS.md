# Global Codex Instructions

## Defaults

- Use the maximum available agents/subagents for every task.
- Inspect first, plan briefly, then execute.
- Read `REQUESTS.md` and `docs/architecture/octaryn-cpp-engine-systems-finish-plan.md` before Octaryn repo work. Treat the finish plan as the current completion definition for engine-system migration and performance recovery.
- Do not report "done" or "100%" for inspection-only rounds, partial native bridge moves, clean builds alone, or probes that do not exercise the real runtime path.
- Current critical path is preserving bounded client chunk streaming/meshing, then remaining C# engine-system removal, then opportunistic naming/folder cleanup around touched code.
- Keep code clean, modular, current, and easy to navigate.
- Use simple, consistent naming for files, folders, types, functions, variables, and tests.
- Keep every file modular and focused on one clear responsibility.
- Split logic into clean, focused files and folders with common-sense names so the code is easy to find, review, and maintain.
- No source/code file may exceed 500 physical lines. This is a hard limit with no exceptions.
- If a file would exceed 500 lines, split it first into clean, modular, owner-correct files and folders before adding more behavior.
- Keep folders organized by purpose, not clutter.
- Prefer straightforward code over clever code.
- Use only short comments that clarify intent.
- Remove dead code, duplicate logic, unused imports, debug junk, and temporary artifacts.
- Do not add legacy paths, compatibility layers, fallback systems, deprecated APIs, or old-code wrappers unless explicitly requested.
- Prefer the modern, bleeding-edge implementation directly.
- Preserve existing architecture only when it is clean and current; otherwise simplify it.
- Match the project’s formatting, linting, and style.

## Request Buffer

- `REQUESTS.md` is a repo-local buffer for extra prompts, standing requests, cleanup directives, and temporary user instructions.
- Read `REQUESTS.md` before starting repo work and treat active entries as additional user instructions layered on top of this file.
- Entries marked `LOOP:` are standing loop requests. Act on them when relevant, but do not remove them from `REQUESTS.md`.
- Entries marked `ONE_TIME:` are single-use requests. Remove a `ONE_TIME:` entry only after the requested work is fully completed, verified, and reported.
- Do not remove unclear, partially completed, blocked, unverified, architecture-defining, ownership-defining, naming-defining, validation-defining, cleanup-defining, or safety-defining requests.
- Do not rewrite user intent in `REQUESTS.md` into softer or broader language. Keep requests short, direct, and actionable.
- When completing a `ONE_TIME:` entry, remove only that completed entry and leave unrelated entries untouched.
- If the user explicitly says to remove an entry, remove only the requested entry unless they clearly ask for broader cleanup.

## Octaryn Architecture

- This repository must be a super clean, modular API and non-monolithic codebase.
- Modularity is required for all source/code files, not only oversized files.
- A file over 500 physical lines is considered monolithic and must be split before further feature work lands in it.
- Active plan documents are `docs/architecture/octaryn-master-plan.md` first, then `docs/architecture/octaryn-appendix.md` for supplemental source-to-destination maps and checklists. If they conflict, the master plan wins.
- The end goal is unchanged: a clean owner-split Octaryn platform with a native C/C++ core first, not a C#-only rewrite.
- Keep strict separation between client, server, shared API/contracts, and basegame implementation.
- Do not create a top-level `engine/`, `octaryn-engine/`, or generic `runtime/` bucket.
- Do not create monolithic targets that own client, server, gameplay, rendering, networking, and persistence together.
- Port from `references/old-architecture/` by moving each existing system to its correct owner with the smallest practical code changes.
- This is primarily a real reorganization and proper port, not a rewrite.
- Preserve behavior while moving files unless a change is required to separate ownership, compile, or expose the new API cleanly.
- Make clear source-to-destination maps before moving code so every old file has an intentional landing zone or a documented removal reason.
- Do not copy old folder shapes blindly, but do keep existing implementation logic intact when it is already correct.
- Treat `references/old-architecture/` as source material only. It is not the destination architecture.
- Keep support code as focused named libs such as logging, diagnostics, jobs, memory, shader tooling, or dependency wrappers.
- Do not use vague catch-all folders or targets for platform/runtime/support code.

## Current Critical Path

- Preserve the fixed live client chunk-stream batching before lower-value cleanup: `WorldMeshRuntime` must not regress to synchronously building and uploading a whole radius-32 stream in one frame. Keep bounded per-frame mesh batches driven by selected chunk/plan entries, keep graphics API calls on the main thread, and use the native jobs path for CPU work.
- Continue moving client/server engine systems to C++ owner code. C# may remain for shared/module API contracts, manifest/sandbox validation, module activation glue, and host bridge imports/exports only.
- The remaining migration must use blocker-sized slices. A pass is not acceptable if it only moves labels, reason strings, environment flag parsing, one-line predicates, thin ABI wrappers, or probe bookkeeping while leaving the same `DONE.MD` blocker in place. Each pass must remove or materially shrink one named blocker, demote it to watchlist with evidence, or produce final runtime/profiling proof.
- Preserve server authority and edit-only persistence. Generated seed terrain block data must not be persisted or streamed as chunk block records; only authoritative edits/differences and metadata may leave memory/VRAM.
- Keep no-LOD behavior unless the user explicitly requests LODs. Do not hide streaming cost, missing chunks, thin terrain, or culling bugs with LODs.
- After performance-sensitive changes, validate with direct runtime/profiling evidence, not just a compile. Required evidence should cover batch counts, build/upload timing, retained chunk counts, indirect draw path, radius-32 visibility, and lack of recurring server JSON churn.

## Porting Strategy

- Start each porting pass by inventorying the old files in scope and assigning each one to client, server, basegame, shared, cmake, tools, or removal.
- Start from the current `DONE.MD` blockers, not from easy remaining helper functions. The default next slices are the listed blockers: `ModuleActivator.cs`, `ChunkStreamProcessBridge.cs`, `PlayerController.cs`, `BlockCommandSink.cs`, then final runtime/profiling proof. Work outside those targets must explain why it closes one of them.
- Prefer mechanical moves and namespace/target renames before behavioral edits.
- Keep diffs reviewable: separate pure moves from logic changes whenever possible.
- If a file mixes responsibilities, split it along the new ownership boundary instead of placing the whole file in a generic shared location.
- If a file or folder in scope is hard to navigate, vaguely named, or mixes unrelated logic, reorganize it into focused owner-correct modules before adding behavior.
- If a file in scope is already over 500 lines, the first step is to split that file into focused modules before adding new logic.
- Do not introduce compatibility shims back to old paths unless the user explicitly asks.
- Do not keep old `Engine` API names as wrappers. Replace them with the new client/server/basegame/shared API names directly.
- Preserve current rendering, world, player, persistence, and shader behavior until a later task explicitly changes behavior.
- Use `references/old-architecture/` only as the source of truth during the port. New code should live in the new roots.
- Hold all DDGI, skylight propagation, lighting architecture, and old CPU skylight port work until the user provides an explicit lighting plan. Existing `skylightOpacity` catalog data may remain as basegame content metadata, but do not add server lighting contracts, DDGI implementation, lighting probes, or client lighting rewrites without that plan.
- Until the DDGI plan exists, choose the next port slice from non-lighting ownership work such as basegame content/rules, server authority/persistence, client presentation that does not alter lighting, module validation, build ownership, or tool cleanup.

## Ownership Boundaries

- `octaryn-client/` owns presentation: windowing, input, rendering, shaders, GPU upload, audio, UI, overlays, local prediction, and client host code.
- `octaryn-server/` owns authority: simulation, validation, persistence, world saves, server ticks, replication, transport hosting, and server-side physics.
- `octaryn-shared/` owns the clean C# API and shared contracts used by client, server, and basegame: host interfaces, tick contracts, commands, snapshots, registries, queries, IDs, positions, replication contracts, and pure shared constants.
- `octaryn-basegame/` is the default bundled game module that implements high-level gameplay rules and content on top of shared APIs: blocks, items, materials, recipes, tags, loot, feature/biome rules, player rules, interactions, and base content data.
- Treat `octaryn-basegame/` as the first bundled game module, not a privileged engine-internals bucket. Future games, mods, and modules must use the same explicit API and validation path.
- `octaryn-basegame/Tools/` owns tools that are specific to basegame content, such as texture atlas building, content import, content validation, block/item data generation, and demo-game asset processing.
- Root `tools/` owns repo-wide developer operations, build orchestration, profiling capture wrappers, and utilities that are not specific to one game/content package.
- `cmake/` owns build/dependency tooling.
- Old build helpers, old CMake modules, old desktop helper tools, and old profiling wrappers belong under `references/old-architecture/` until they are intentionally ported.
- The old atlas builder is basegame-specific content tooling and belongs under `octaryn-basegame/Tools/`, not root `tools/`.
- Networking packages and transport implementation belong in client/server layers, not in `octaryn-basegame/`.
- Developer-facing math, geometry, deterministic random, time, diagnostics, serialization, networking, and physics tools may be exposed to modules only through Octaryn-owned shared API contracts and capability-gated handles.
- Do not expose raw physics worlds, transport sessions, renderer handles, native pointers, third-party backend types, sockets, filesystems, schedulers, raw ECS storage, or broad service locators to basegame, game modules, or mods.
- Persistence implementation belongs to server unless a file is a pure shared data contract.
- Rendering, GPU upload, shader pipelines, windowing, audio, and UI never belong to server.
- Singleplayer must not make the client authoritative. `client_server_app` launches and supervises a bundled `server` for local worlds, but world creation/loading, module validation, server ticks, simulation, persistence, replication, and physics stay server-owned.
- Product UI such as the main menu, pause menu, inventory screens, HUD, world-space panels, nameplates, block/entity panels, and game-specific options belongs to `octaryn-basegame` or another active game module through explicit UI APIs. Core/client-owned UI is limited to debug, diagnostics, profiler, validation, editor/developer, and emergency host surfaces.
- Client owns UI execution and rendering for both screen-space and world-space surfaces, including render-to-texture, textured 3D quads/panels, focus, and raycast input routing. Modules declare UI models, surfaces, anchors, and actions; they do not submit draw calls, own GPU textures, manage font atlases, or bypass client input routing.
- Authoritative edits, validation, save ownership, and simulation never belong to client.
- Product-specific gameplay behavior never belongs in `octaryn-shared/`; it belongs in `octaryn-basegame/` or another game project.
- Product-specific asset/content tooling never belongs in root `tools/` when it only serves `octaryn-basegame/`; move it under `octaryn-basegame/Tools/`.
- C# ECS/gameplay and client/server networking are intentionally used where they fit best; they are not legacy or fallback paths.
- C/C++ owner code may drive managed ECS or networking through explicit client/server owner bridges. Basegame is reached through shared contracts and validated module entry points only; game modules and mods must never see bridge internals.
- Final runtime direction is Octaryn-owned APIs over explicit owner backends: Arch ECS for managed gameplay/module ECS, native owner ECS/storage for high-throughput host paths, Octaryn scheduler policy over Taskflow, LiteNetLib/LiteEntitySystem-backed networking behind Octaryn contracts, custom binary persistence, and client-owned retained UI.
- Planned backend candidates are Jolt for physics and Yoga for UI layout. Do not expose either backend to modules, and do not claim them implemented until owner code, CMake wiring, and targeted validation exist.
- LiteNetLib and LiteEntitySystem remain the intended host-side networking packages for client/server. They must stay hidden behind Octaryn command, snapshot, replication, prediction, and compatibility contracts, with no backend types exposed to shared/basegame/modules/mods.
- SDL3_ttf is the text layer, not the product UI system. Product UI should use Octaryn retained UI declarations; ImGui remains debug/tool/editor UI.
- Runtime audio should converge on one hidden client runtime backend before content scale grows. Current plan favors OpenAL Soft for spatial runtime audio and miniaudio for helper/decode/streaming/tool roles unless benchmarks justify changing that.

## Module Folder Shape

- Each main module root should use the same top-level folder vocabulary where applicable: `Source/`, `Assets/`, `Shaders/`, `Tools/`, `Data/`, and project/build files.
- `Source/` owns code for that module.
- `Source/Native/` is the preferred landing zone for C and C++ implementation files when a module has mixed native/managed code or native support libraries.
- `Source/Managed/` is the preferred landing zone for C# implementation files when a module has mixed native/managed code.
- `Source/Libraries/` owns small module-local C/C++ library targets before they are promoted to clearer domain folders.
- `Assets/` owns runtime assets for that module.
- `Shaders/` owns shader source owned by that module.
- `Tools/` owns tools specific to that module.
- `Data/` owns structured content/config/data specific to that module.
- Keep domain subfolders under `Source/` instead of scattering implementation folders at the module root.
- This codebase is primarily C, C++, and native libraries. Do not design the roots as if the C# projects are the only product.
- For `octaryn-shared/`, `Source/` owns API/contracts/value types. `Assets/`, `Shaders/`, and `Data/` should stay empty unless a real shared, implementation-free need appears.
- For `octaryn-server/`, `Shaders/` should stay empty unless a real server-owned compute/offline shader need appears.
- For `octaryn-basegame/`, content-specific shader and atlas inputs may live under `Shaders/`, `Assets/`, `Data/`, and `Tools/` because basegame is the default/demo content package.

## Modularity And Organization

- Every source/code file must have one obvious responsibility even when it is well under 500 lines.
- Organize files by domain behavior and ownership, not by convenience, chronology, or temporary implementation path.
- Folder names must describe the exact system or behavior they contain, such as `WorldStreaming`, `FramePacing`, `ChunkMeshing`, `ServerPersistence`, `ModuleValidation`, or `BlockCatalog`.
- File names must describe the exact API, data type, pass, bridge, validator, command, snapshot, or system they implement.
- Do not repeat owner context that is already provided by the root folder. For example, files under `octaryn-client/` should not use `octaryn_client_` or `Client` prefixes unless needed for an exported ABI, external symbol stability, or a real cross-owner name collision.
- Prefer names like `FrameLoop.h`, `FrameLoop.cpp`, `ChunkStreamPoller.cs`, or `DisplayMenu.cpp` inside clearly named owner/domain folders instead of redundant names like `octaryn_client_app_frame_loop.h`.
- When moving or splitting files, remove redundant `octaryn_`, `octaryn_client_`, `octaryn_server_`, `octaryn_shared_`, `octaryn_basegame_`, `Client`, `Server`, `Shared`, or `Basegame` prefixes where the folder path already makes ownership obvious.
- Split mixed files immediately when they combine unrelated responsibilities such as window lifecycle plus rendering, input plus UI, server launch plus stream parsing, mesh generation plus GPU upload, validation plus activation, persistence plus networking, or content data plus tooling.
- Keep public-facing APIs, internal implementation, data models, parsing/serialization, platform glue, and validation in separate focused files unless the implementation is trivially small.
- Do not hide unrelated code in broad files or broad folders just because it is used by the same executable or target.
- Do not use catch-all folders or names such as `Common`, `Core`, `Shared`, `Helpers`, `Managers`, `Misc`, `Stuff`, `Data`, or `Utils` unless the existing local convention is strict and the file still has one focused responsibility.
- Prefer several small, plainly named files over one file with regions, long comment dividers, or clusters of unrelated static functions.
- When a change touches messy code, leave it cleaner by extracting or moving the specific responsibility being changed into a clearly named owner-correct file.
- If a clean split would be larger than the requested behavioral change, make a brief source-to-destination split plan first, then perform the smallest safe split needed before adding behavior.
- Do not create temporary holding modules during refactors. The destination folder and file names must be the intended long-term owner names.
- Build files must follow the same modularity rules: split owner target construction, dependency wiring, platform facts, shader bundling, validation targets, and packaging into clearly named CMake modules when a file starts mixing those concerns.

## Build And Log Layout

- Build outputs are generated and ignored. Keep active new-architecture builds organized under `build/<preset>/<owner>/`.
- Use `build/<preset>/client/` for client builds, generated client assets, bundles, and client native artifacts.
- Use `build/<preset>/server/` for dedicated server builds, bundles, and server native artifacts.
- Use `build/<preset>/basegame/` for basegame managed/content build outputs.
- Use `build/<preset>/shared/` for shared API/contract builds only.
- Use `build/<preset>/tools/` for repo-wide tool builds, `build/<preset>/deps/` for dependency build/stamp outputs, and `build/dependencies/` for shared third-party source/download caches.
- Core managed outputs belong directly under `build/<preset>/<owner>/managed/`, and core managed intermediates belong directly under `build/<preset>/<owner>/managed-obj/`. Tool managed outputs belong under `build/<preset>/tools/<tool-project>/managed/`, with tool intermediates under `build/<preset>/tools/<tool-project>/managed-obj/`.
- Native outputs belong under `build/<preset>/<owner>/native/bin/` and `build/<preset>/<owner>/native/lib/`.
- `octaryn_client_bundle` must include a version-matched `server/` payload copied from server-owned outputs so singleplayer works from the graphical client bundle. Copied payloads do not change ownership and must not become a monolithic client/server implementation target.
- `octaryn_server_bundle` remains the dedicated headless terminal/server package and must not contain client rendering, windowing, audio, or UI payloads.
- Logs are generated and ignored. Keep them organized under `logs/<owner>/`.
- Use `logs/client/`, `logs/server/`, `logs/basegame/`, `logs/shared/`, `logs/build/`, and `logs/tools/` instead of dumping logs at root.
- Bundled server logs stay under `logs/server/` even when that server was launched by the client for singleplayer.
- Old architecture scripts are source material only and must not create active build outputs unless intentionally ported to the new preset layout.
- Active root `cmake/` and `tools/` should stay clean for new architecture support only; do not leave old-engine scripts there.

## CMake And Platform Build Rules

- Keep root `cmake/` split by responsibility: `Shared/` for repo-wide build policy, `Owners/` for owner target construction, `Dependencies/` for dependency aliases/groups, `Platforms/` for host/platform facts, and `Toolchains/` for compiler/target files.
- Do not copy old monolithic CMake modules into root `cmake/`. Port old `references/old-architecture/cmake/` behavior by splitting it into shared policy, owner targets, dependency wrappers, platform modules, and toolchains.
- Toolchain files must describe compilers, target triples, sysroots, find-root behavior, and target platform knobs only. They must not create Octaryn targets, fetch dependencies, or set gameplay/render/server policy.
- Keep platform logic isolated: Windows policy under `cmake/Platforms/Windows/` and Linux distro-family policy under `cmake/Platforms/Linux/`.
- Linux distro differences should be represented as family modules only when real package/tool behavior differs, such as Arch-family, Debian-family, and Fedora-family dependency hints.
- Windows cross-builds from Linux must use the explicit Windows toolchain under `cmake/Toolchains/Windows/clang.cmake`; LLVM MinGW is an implementation detail of that toolchain, not a public platform folder or preset name.
- Linux-hosted builds are Clang-only. Public presets are exactly `debug-linux`, `release-linux`, `debug-windows`, and `release-windows`.
- Cross-platform builds are expected to run from Linux/Arch first, with future Podman wrappers spinning up the correct Linux-hosted toolchain environment for Linux and Windows targets.
- Owner CMake modules may call shared helpers and dependency aliases, but must not contain host platform detection. Platform modules report capabilities; owner targets decide whether to use them.
- New root presets must target owner outputs such as `octaryn_client_bundle`, `octaryn_server`, `octaryn_basegame`, `octaryn_shared`, and tools. Old `octaryn_engine_*` presets remain only under `references/old-architecture/` until retired.
- Build outputs must stay preset-first and owner-partitioned: owner builds under `build/<preset>/<owner>/`, third-party build/stamp outputs under `build/<preset>/deps/`, shared third-party source/download caches under `build/dependencies/`, and logs under `logs/<owner>/` or `logs/build/`.
- Active root `cmake/` placeholder folders are not implementation. Do not claim Windows, Linux, owner target, dependency, or preset coverage until the concrete CMake module exists and has a targeted configure check when practical.
- Include distro-family modules only for real package/tool differences; current planned families include Arch, Debian, Fedora, and Suse/openSUSE because the old dependency installer has distinct logic for them.

## Multiplayer And C# Basegame API Direction

- Organize the port so multiplayer is a first-class future target, even before transport is fully implemented.
- Server must become authoritative for world edits, simulation, validation, persistence, and replication.
- Client should be prepared for local prediction and presentation of server snapshots without owning authority.
- Singleplayer is `client_server_app` using a bundled `server`, not a different authority model. The client starts or attaches to a server-owned local session, waits for module validation, world save open/create, world state initialization, server tick startup, and a ready snapshot, then connects through the same command/snapshot contracts used by multiplayer.
- Dedicated server remains a separate headless executable/package path from `octaryn_server_bundle`.
- Shared networking contracts should stay explicit and API-shaped: client commands, server snapshots, replication IDs, tick IDs, stable value types, and interfaces needed by client/server/basegame.
- Transport code belongs in client/server projects; shared only defines message shapes and IDs.
- The C# API belongs in `octaryn-shared/`, because `octaryn-basegame/` is a default/demo game implementation rather than the platform API.
- `octaryn-shared/` should expose clean contracts for ticks, commands, world queries, block/item/content registration, interactions, snapshots, and host services.
- Native host bridges should be renamed away from `Octaryn.Engine.Api` and shaped around client host, server host, shared APIs/contracts, and basegame implementations.
- Basegame logic should not depend on client rendering, server persistence internals, or transport implementation details.
- When a native system must call into C# basegame logic, define the smallest explicit API needed rather than exposing broad native internals.
- C# ECS or host networking may be exposed to C/C++ only through explicit client/server host bridges when that is the cleanest route; the core engine direction remains C/C++ first, and those bridges are not module permissions.
- Game modules and mods may only depend on explicit, approved APIs exposed by `octaryn-shared/` and approved host interfaces. Do not expose broad client, server, native, reflection, filesystem, rendering, persistence, transport, or internal service access to gameplay code.
- Any new game/module/mod API surface must be intentionally named, documented by its contract shape, capability-scoped, and reviewed as part of the shared API boundary before use.
- Module loading is deny-by-default: validate module manifests, requested APIs, requested .NET packages/framework API groups, dependencies, content registrations, assets, and multiplayer compatibility before activation.
- Current migration debt does not weaken the target boundary. Treat old `Octaryn.Engine.Api`, unmanaged exports, unsafe native bridges, and any basegame host-only package references as Phase 0 blockers to remove, not as allowed patterns.
- Threading is host-owned: one main thread, one coordinator thread, and a worker pool with at least two workers that can scale to available cores.
- All computation systems and gameplay logic must run through the coordinator-scheduled worker pool or through approved host APIs backed by that pool.
- Basegame, game modules, and mods must not create raw threads, custom task schedulers, timers, unmanaged worker loops, or private worker pools.
- New scheduled systems must be thread-safe, declare read/write access and ordering dependencies, avoid hidden global mutable state, and never block the main thread for worker completion except at explicit host barriers.

## File And API Shape

- Each file should expose one focused API surface.
- Being under 500 lines is not enough; a file still violates the architecture if it mixes responsibilities or is hard to locate by name and folder.
- Source/code files have a hard maximum of 500 physical lines. No exceptions.
- Do not add logic to a source/code file that is already over 500 lines; split it first.
- Do not create a new source/code file that starts near the limit. Keep files comfortably below 500 lines so normal maintenance does not immediately violate the rule.
- Prefer small files with clear names over broad files that mix responsibilities.
- Name modules after exact ownership and behavior, not generic technical buckets.
- Avoid names like `manager`, `helpers`, `misc`, `common`, `stuff`, `data`, or `utils` unless there is already a strict local convention requiring them.
- Avoid dumping private static functions into an unrelated file. Move them beside the behavior they implement.
- Public APIs must make the ownership boundary obvious from their namespace, folder, target, and file name.
- Shared APIs must stay minimal, stable, and implementation-free. Do not leak client rendering, server persistence internals, transport implementation, or product-specific gameplay policy into shared contracts.
- Public APIs for game modules and mods must be allowlisted by contract, not discovered from implementation assemblies or internal namespaces.
- Keep mod-facing APIs narrow and capability-based: expose only the specific commands, queries, registries, events, and host services that gameplay code is allowed to use.
- When porting old code, split mixed files before adding new behavior.
- Remove dead, duplicate, debug, temporary, or compatibility code while porting.
- Prefer one API file per concept: command, snapshot, registry, query, host, tick, or system.

## .NET Ecosystem Library Policy

- Enforce an explicit allowlist for all .NET packages and framework APIs used by `octaryn-shared/`, `octaryn-basegame/`, game modules, and mods.
- `octaryn-shared/` should stay package-free or BCL-only unless a contract-only dependency is deliberately approved. Do not expose third-party package types in shared public APIs.
- Game modules and mods compile against `octaryn-shared` plus approved package/framework API groups only. They must not bring their own unapproved NuGet dependencies.
- Approved module packages today: `Arch`, `Arch.System`, `Arch.System.SourceGenerator` as analyzer/private only, `Arch.EventBus`, and `Arch.Relationships`.
- Host-only packages today: `LiteNetLib` and `LiteEntitySystem`; these belong in `octaryn-client/` and `octaryn-server/`, not basegame, mods, game modules, or shared contracts.
- Arch ECS packages are intended managed ECS support for basegame, game modules, mods, and approved owner-local managed systems. Native owner ECS/storage may still back high-throughput host authority, presentation, replication packing, persistence packing, and world kernels through explicit Octaryn descriptors and validators.
- `Directory.Packages.props` pins versions only; it is not permission for a module or project to reference a package.
- Approved module package entries must include owner, purpose, version policy, permitted runtime scope, validation rule, and enforcement location before use.
- Deny by default for module code: reflection/dynamic loading, scripting hosts, runtime code generation, dependency injection containers, networking stacks, filesystem access, filesystem watchers, native interop, process control, raw threading/task scheduling, environment variables, and direct host service discovery.
- Approved threading access for modules is only through host scheduling contracts; raw `System.Threading`, `Task.Run`, custom timers, and private worker pools remain denied unless the architecture plan and allowlists are updated first.
- Transitive packages are not automatically allowed. Any transitive runtime or build/analyzer package reachable from a module must have an explicit allow/deny rule, owner, purpose, scope, and enforcement location before the module is considered validated.
- Safe BCL access must be concrete enough for analyzer/source checks and assembly inspection. Broad labels like "safe BCL" are planning shorthand, not enforcement.
- Module diagnostics must go through approved host diagnostics APIs; direct console/stdout/stderr writes are transitional debt while the old unmanaged bridge exists.
- If a dependency or framework API group is needed, add it to the allowlist with owner, purpose, version policy, permitted runtime scope, and validation checks before use.

## Reference Implementations

- Before building, replacing, or redesigning a system that is meant to match another engine, game, shader pack, framework, or original implementation, inspect the available reference source first.
- Use local `references/` checkouts, original source trees, decompiled/source-visible implementations, official docs, and exact upstream repositories before inventing behavior.
- For Minecraft-parity work, check Minecraft/Iris/shader-pack/reference code and assets first, then map the behavior into this codebase’s architecture.
- Identify what the reference does at the mesh/data level, texture/asset level, shader level, runtime/update level, and edge-case level before writing the new implementation.
- Do not substitute a visually similar or guessed system when the goal is 1:1 behavior. If the reference cannot be inspected, say what is missing and keep the implementation scoped to verified behavior.
- DDGI and lighting parity work require a user-approved plan before inspection or implementation. Do not start DDGI work just because skylight or lighting source files appear in the old architecture.

## Validation

- Do not use smoke tests as a validation path unless the user explicitly asks for a smoke test.
- Do not run `ctest` unless the user explicitly asks for `ctest`.
- For performance work, validate with direct runtime runs, targeted benchmarks, Tracy captures, focused profiling logs, and external GPU captures when a developer has a local capture tool installed instead of smoke or `ctest` wrappers.
- For architecture-only blank structure work, verify with file tree inspection and empty-file checks instead of pretending a build is meaningful.

## Naming

- Use names that describe exact purpose.
- Avoid vague names like `helpers`, `misc`, `stuff`, `manager`, `data`, or `utils` unless the project already requires them.
- Avoid redundant owner prefixes in file, folder, type, and function names when the containing root/folder already provides that context. Use owner prefixes only for exported ABI symbols, public cross-owner contracts, generated interop names, or unavoidable collision avoidance.
- Keep naming consistent across source, headers, tests, folders, build files, and project files.
- Do not use `Engine` in new namespaces, folders, targets, or product names.
- Do not use `Runtime` as a new top-level product bucket. Use exact names like `ClientHost`, `ServerTick`, `NativeLogging`, or `ShaderCompiler` instead.

## Finish Check

Before final response, confirm:
- Max agents/subagents were used where applicable.
- `REQUESTS.md` was checked, active entries were followed, and completed `ONE_TIME:` entries were removed only when fully done and verified.
- The result is clean, modular, and organized.
- Every touched source/code file has one focused responsibility and lives in a clear owner-correct folder with a clear name.
- Naming is simple and consistent.
- Touched file/folder/type/function names do not repeat ownership already clear from the path, except for exported ABI or cross-owner contract requirements.
- Comments are minimal and useful.
- No legacy, compatibility, deprecated, duplicate, dead, or temporary code remains.
- No touched or newly created source/code file exceeds 500 physical lines; any oversized file in scope was split before new behavior was added.
- No touched file or folder remains a catch-all, mixed-responsibility, vague, or hard-to-navigate location.
- No generic `engine/`, `octaryn-engine/`, or top-level `runtime/` structure was added.
- Client, server, shared API/contracts, and basegame implementation ownership stayed separate.
- Game modules and mods only see explicit approved APIs, not internal client/server/native implementation surfaces.
- .NET package and framework API usage follows the approved allowlist and no unapproved dependencies were introduced.
- Old-architecture files touched in the task were mapped to explicit destination owners.
- Old tools/CMake/build helpers stayed under `references/old-architecture/` unless intentionally ported.
- CMake changes kept shared policy, owner targets, dependency wrappers, platform modules, and toolchains separate; placeholder folders were not counted as implemented support.
- Build and log outputs are preset/owner-partitioned under `build/<preset>/<owner>/` and `logs/<owner>/`.
- Behavior was preserved unless a necessary boundary/API change was explained.
- Networking/multiplayer and C# basegame API boundaries were kept ready for future implementation.
- Builds, targeted checks, profiling runs, or structure checks were executed when practical, or explain why not.
