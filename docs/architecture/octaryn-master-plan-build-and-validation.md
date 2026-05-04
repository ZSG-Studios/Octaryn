# Octaryn Master Plan: Build And Validation

This file is part of the canonical `octaryn-master-plan.md` policy set. It owns source migration, build/platform, capability, dependency, and validation policy.

## Source-To-Destination Migration Map

Every port slice starts with an old-source inventory and an explicit destination or removal reason.

Client source candidates:

- `old-architecture/source/app/` window/application startup pieces -> focused `octaryn-client/Source/` behavior folders such as `Source/App/`, `Source/HostBridge/`, or `Source/Window/` by exact concern.
- `old-architecture/source/rendering/`, `old-architecture/source/gpu/`, render upload paths, shader pipeline setup -> `octaryn-client/Source/Rendering/`, `octaryn-client/Shaders/`, or focused client native libraries.
- Client-side input, camera, display, audio, overlays, debug UI -> `octaryn-client/Source/Input/`, `Source/Audio/`, `Source/Ui/`, `Source/WorldPresentation/`, or client debug/tool surfaces.
- Client-side mesh planning, presentation snapshots, and upload descriptors -> `octaryn-client/Source/WorldPresentation/`.
- Exclude DDGI, skylight propagation, and lighting rewrites until the dedicated lighting plan exists.

Server source candidates:

- `old-architecture/source/world/edit/` -> `octaryn-server/Source/World/Blocks/` or `World/Queries/`.
- Server-side pieces of `old-architecture/source/world/runtime/`, `world/chunks/`, `world/jobs/`, and `world/generation/` -> `octaryn-server/Source/World/`, `Simulation/`, or `Tick/`.
- `old-architecture/source/physics/` -> `octaryn-server/Source/Physics/` for authority and client prediction wrappers only where explicitly planned.
- Server-owned persistence pieces from `old-architecture/source/core/persistence/` -> `octaryn-server/Source/Persistence/`.
- Exclude DDGI, skylight propagation, lighting architecture, and old CPU skylight behavior until the dedicated lighting plan exists.

Basegame source candidates:

- High-level content definitions from `old-architecture/source/world/block/` -> `octaryn-basegame/Data/`, `Source/Content/`, or generator-only metadata after stripping storage, lighting, mesh, and old host-state details.
- Texture atlas and content import behavior serving basegame content -> `octaryn-basegame/Tools/`.
- Player rules, interaction rules, item/block rules, recipes, tags, loot, features, biome rules, and product UI -> `octaryn-basegame/Source/Gameplay/`, `Source/Content/`, `Source/Ui/`, `Data/`, or `Assets/`.
- Existing `skylightOpacity` values may stay as basegame block metadata, but must not become DDGI, skylight propagation, or lighting host contracts until the lighting plan exists.

Shared source candidates:

- Pure IDs, value types, world bounds, positions, directions, block/entity references, command/snapshot records, host API IDs, capability IDs, manifest records, validation result shapes, and ABI layout/version records -> `octaryn-shared/Source/`.
- Shared must not receive implementation, scanners, asset processors, persistence engines, transport code, renderer code, or gameplay policy.

Tools and CMake candidates:

- Old build helpers, old CMake modules, old desktop helper tools, and old profiling wrappers stay under `old-architecture/` until intentionally ported.
- Old atlas builder -> `octaryn-basegame/Tools/` because it is basegame-specific content tooling.
- Repo-wide validators, profiling wrappers, build orchestration, package checks, shader tooling, and developer operations -> root `tools/`.
- Old RenderDoc helpers stay in `old-architecture/`; RenderDoc is an external developer tool.

## CMake And Platform Architecture

Root `cmake/` is split by responsibility:

- `cmake/Shared/`: repo-wide CMake defaults, warning policy, output layout, build/log owner paths, shared helper functions, and naming rules.
- `cmake/Owners/`: target construction for client, server, shared contracts, basegame assets/modules, and tools. Owner modules may call shared helpers and dependency aliases but must not contain platform detection.
- `cmake/Dependencies/`: dependency wrappers and allowed dependency aliases grouped by real owner need, not a global old dependency bag.
- `cmake/Platforms/`: host platform facts and distro-family policy.
- `cmake/Toolchains/`: compiler, sysroot, target triple, find-root behavior, and platform knobs only. Toolchain files must not create Octaryn targets or fetch dependencies.

Platform rules:

- Active configure presets are exactly `debug-linux`, `release-linux`, `debug-windows`, and `release-windows`.
- Linux-hosted builds are Clang-only in active lanes.
- Windows targets are cross-built from Linux/Arch using `cmake/Toolchains/Windows/clang.cmake`.
- LLVM MinGW is an implementation detail of the Windows toolchain, not a public platform folder or preset name.
- Active Podman build wrappers use the Linux-hosted toolchain environment for Linux and Windows targets; expand those wrappers in place instead of adding host-specific build layouts.
- Linux distro policy is split by family only when real package/tool behavior differs. Current planned families are Arch, Debian, Fedora, and Suse/openSUSE.
- Platform modules report capabilities. Owner targets decide whether to use them. Platform modules must not own gameplay, rendering, server, basegame, or sandbox behavior.

Old CMake port map:

- `old-architecture/cmake/BuildLayout.cmake` -> `cmake/Shared/OwnerBuildLayout.cmake`, after renaming away from old product names and enforcing owner build/log paths.
- Old dependency cache paths like `build/shared/deps/<bucket>` and `logs/deps/<bucket>` -> `build/dependencies/` plus preset-specific `build/<preset>/deps/`.
- `old-architecture/cmake/ProjectOptions.cmake` -> `cmake/Shared/ProjectDefaults.cmake` plus owner-specific options.
- `old-architecture/cmake/Dependencies.cmake` -> `cmake/Dependencies/`, split by policy, aliases, and owner groups.
- `old-architecture/cmake/CPM.cmake` -> `cmake/Dependencies/` only if CPM remains selected.
- `old-architecture/cmake/toolchains/windows-x64.cmake` -> `cmake/Toolchains/Windows/clang.cmake`; GCC MinGW is not an active lane.
- `old-architecture/CMakePresets.json` -> root presets only after the owner/platform/toolchain split exists.
- Old build scripts -> root `tools/build/` only after they select new owner presets and write only to approved build/log paths.

## Build Target Inventory

Active root targets:

```text
octaryn_shared
octaryn_shared_native
octaryn_shared_host_abi
octaryn_native_logging
octaryn_native_diagnostics
octaryn_native_memory
octaryn_native_profiling
octaryn_native_jobs
octaryn_basegame
octaryn_basegame_native
octaryn_basegame_bundle
octaryn_server
octaryn_server_bundle
octaryn_server_native
octaryn_server_managed_bridge
octaryn_server_launch_probe
octaryn_client_managed
octaryn_client_native
octaryn_client_asset_paths
octaryn_client_app_settings
octaryn_client_camera
octaryn_client_camera_matrix
octaryn_client_display_catalog
octaryn_client_display_menu
octaryn_client_display_settings
octaryn_client_frame_pacing
octaryn_client_fullscreen_display_mode
octaryn_client_frame_metrics
octaryn_client_hidden_block_uniforms
octaryn_client_host_environment
octaryn_client_lighting_settings
octaryn_client_render_distance
octaryn_client_shader_creation
octaryn_client_shader_metadata_contract
octaryn_client_shaders
octaryn_client_swapchain
octaryn_client_visibility_flags
octaryn_client_window_frame_statistics
octaryn_client_window_lifecycle
octaryn_client_managed_bridge
octaryn_client_launch_probe
octaryn_client_server_app
octaryn_client_bundle
octaryn_tools
octaryn_shader_compiler
octaryn_debug_tools
octaryn_all
octaryn_validate_all
octaryn_validate_cmake_targets
octaryn_validate_cmake_policy_separation
octaryn_validate_cmake_dependency_aliases
octaryn_validate_package_policy_sync
octaryn_validate_project_references
octaryn_validate_module_manifest_packages
octaryn_validate_module_manifest_files
octaryn_validate_module_manifest_probe
octaryn_validate_bundle_module_payload
octaryn_validate_client_server_app
octaryn_client_server_app_launch_probe
octaryn_validate_client_shader_bundle
octaryn_validate_module_source_api
octaryn_validate_module_binary_sandbox
octaryn_validate_module_layout
octaryn_validate_basegame_block_catalog
octaryn_validate_basegame_worldgen_content
octaryn_validate_dotnet_package_assets
octaryn_validate_native_abi_contracts
octaryn_validate_native_owner_boundaries
octaryn_validate_native_archive_format
octaryn_validate_dotnet_owners
octaryn_validate_world_time_probe
octaryn_validate_owner_module_validation_probe
octaryn_validate_server_world_blocks_probe
octaryn_validate_server_world_generation_probe
octaryn_validate_basegame_player_probe
octaryn_validate_basegame_interaction_probe
octaryn_validate_hostfxr_bridge_exports
octaryn_validate_owner_launch_probes
octaryn_run_client_launch_probe
octaryn_run_server_launch_probe
```

Internal dependency targets such as `octaryn_dotnet_hosting` and `octaryn_native_threads` are implementation details for owner CMake modules. They are not public build targets and must stay under dependency/owner modules rather than old-architecture target wiring.

Planned focused targets are added only when implementation exists:

```text
octaryn_client_assets
octaryn_basegame_assets
octaryn_module_validator
octaryn_module_sandbox_contracts
```

## Capability Model

Capabilities are deny-by-default and explicit.

Examples:

- `content.blocks`
- `content.items`
- `content.entities`
- `content.override`
- `content.ui`
- `gameplay.systems`
- `gameplay.interactions`
- `world.queries.read`
- `world.blocks.edit.intent`
- `entities.spawn.intent`
- `inventory.mutate.intent`
- `ui.contribute`
- `input.actions`
- `math.core`
- `geometry.queries`
- `random.deterministic`
- `time.tick`
- `diagnostics.module`
- `physics.declare`
- `physics.query`
- `physics.intent`
- `network.replicated_components`
- `network.messages`
- `persistence.components`
- `native.systems`
- `world.fluids.read`
- `world.fluids.edit.intent`
- `world.gases.read`
- `world.gases.edit.intent`
- `worldgen.biomes`
- `worldgen.features`
- `worldgen.noise`
- `content.recipes`
- `content.loot`
- `content.tags`
- `content.assets`
- `content.localization`
- `ui.theme`
- `ui.worldspace`
- `audio.emit`
- `animation.contribute`
- `save.schemas`
- `save.migrations`
- `multiplayer.compat`
- `mod.dependencies`
- `asset.hashes`
- `admin.commands`
- `permissions.roles`
- `probes.readonly`
- `editor.tools`

Capabilities should be specific enough that a module can be approved for item definitions without gaining entity spawning, filesystem access, networking, renderer access, or native code.

Capability families:

| Family | Examples |
| --- | --- |
| Content | `content.blocks`, `content.items`, `content.entities`, `content.recipes`, `content.loot`, `content.tags`, `content.assets`, `content.localization` |
| Gameplay | `gameplay.systems`, `gameplay.interactions`, `inventory.mutate.intent`, `entities.spawn.intent` |
| World | `world.queries.read`, `world.blocks.edit.intent`, `world.fluids.read`, `world.fluids.edit.intent`, `world.gases.read`, `world.gases.edit.intent`, `worldgen.biomes`, `worldgen.features`, `worldgen.noise` |
| Presentation | `ui.contribute`, `ui.theme`, `ui.worldspace`, `input.actions`, `audio.emit`, `animation.contribute` |
| Host contracts | `physics.declare`, `physics.query`, `physics.intent`, `network.replicated_components`, `network.messages`, `persistence.components` |
| Compatibility | `save.schemas`, `save.migrations`, `multiplayer.compat`, `asset.hashes`, `mod.dependencies` |
| Operations | `diagnostics.module`, `admin.commands`, `permissions.roles`, `probes.readonly`, `editor.tools`, `native.systems` |

Capabilities grant only Octaryn API access. They never grant raw backend access.

## Dependency Decisions

| Dependency or candidate | Decision | Public API rule |
| --- | --- | --- |
| Arch, Arch.System, Arch.EventBus, Arch.Relationships | Keep as managed gameplay/module ECS stack. | Allowed only through approved package policy; Arch stays module/owner-local and no Arch types may appear in `octaryn-shared` public contracts. |
| Taskflow | Keep as hidden job execution substrate. | Modules see Octaryn scheduling contracts only. |
| LiteNetLib | Keep as hidden transport backend. | Modules see Octaryn networking contracts only. |
| LiteEntitySystem | Keep as hidden client/server implementation component where it fits. | Octaryn replication descriptors, protocol, authority, and public contracts remain canonical; no LiteEntitySystem entity/RPC/SyncVar concepts leak into shared/module APIs. |
| Jolt | First physics backend candidate. | Modules see Octaryn physics declarations, queries, events, and intents only. |
| PhysX | Deferred candidate only by user-approved physics plan. | No active dependency or API surface. |
| Yoga | First layout solver candidate. | Modules see Octaryn UI declarations only. |
| RmlUi | Deferred candidate only by user-approved UI authoring plan. | No active dependency or API surface. |
| SDL3 GPU, SDL3_ttf | Keep in client presentation/text paths. | No raw SDL window, renderer, GPU, font, or event handles in module APIs. |
| ImGui stack | Debug/tool/editor UI only. | Not basegame product UI. |
| OpenAL Soft | Favored hidden runtime audio backend. | Modules declare audio events; no backend handles. |
| miniaudio | Helper/decode/streaming/tool roles unless benchmarks choose otherwise. | No backend handles in module APIs. |
| Glaze | JSON metadata/manifests/tooling. | Shared contracts stay BCL/value-shape only unless a contract-only dependency is approved. |
| LZ4 | Hot chunk/cache compression. | Server/tools implementation detail. |
| Zstd | Cold saves, backups, bulk transfer. | Server/tools implementation detail. |
| FlatBuffers | Optional control-plane envelope candidate. | Not dense voxel/chunk/entity save format. |
| Protobuf, Cap'n Proto | Not primary world save formats. | Do not build the save model around them. |
| Taffy | Deferred. | Rust/FFI/toolchain complexity; reconsider only if Yoga cannot satisfy layout needs under a user-approved UI layout plan. |
| fastgltf, KTX, meshoptimizer, ozz-animation, shaderc, SPIR-V tools, SPIRV-Cross/Shadercross | Keep for assets/shaders/animation tooling and client presentation as appropriate. | Modules declare assets/materials/animations through Octaryn contracts. |
| Recast/Detour | Later navigation candidate. | Defer until world, physics, persistence, and tooling spines are stable. |
| EnTT, Flecs, Nuklear | Not planned public-core choices. | Do not introduce as module-facing architecture. |

### Managed Package And Framework Allowlist

`octaryn-shared` stays package-free or BCL-only unless a contract-only dependency is deliberately approved here. `Directory.Packages.props` pins versions; it is not permission for a module or project to reference a package.

| Package or API group | Allowed owners | Purpose | Runtime scope | Validation rule |
| --- | --- | --- | --- | --- |
| `Arch` | `octaryn-basegame`, approved modules/mods, `octaryn-client`, `octaryn-server` | Managed ECS for gameplay, basegame, modules, mods, and approved owner-local worlds. | Module implementation and owner-local host integration only. | Exact central pin; no public shared API types; bridges meet native storage through Octaryn descriptors. |
| `Arch.LowLevel` | Transitive package for approved Arch runtime packages. | Low-level Arch runtime support. | Transitive module runtime only. | May not be referenced directly unless promoted to an explicit approved package. |
| `Arch.System` | `octaryn-basegame`, approved modules/mods, `octaryn-client`, `octaryn-server` | Managed ECS system authoring/execution support driven by Octaryn host scheduling declarations. | Module implementation and owner-local host integration only. | Exact central pin; no public shared API types; systems declare Octaryn phases and read/write sets. |
| `Arch.System.SourceGenerator` | Projects that directly define Arch systems. | Compile-time ECS system generation. | Build/analyzer only. | Must be requested as build package with private analyzer assets only. |
| `Arch.EventBus` | `octaryn-basegame`, approved modules/mods, owner-local client/server systems. | Gameplay and host integration events. | Module implementation and owner-local host integration only. | Exact central pin; no public shared API types. |
| `Arch.Relationships` | `octaryn-basegame`, approved modules/mods, owner-local client/server systems. | Gameplay and host entity relationships. | Module implementation and owner-local host integration only. | Exact central pin; no public shared API types. |
| `Collections.Pooled` | Transitive package for approved Arch runtime packages. | Pooled collection implementation used by Arch. | Transitive module runtime only. | No direct module reference unless explicitly promoted. |
| `CommunityToolkit.HighPerformance` | Transitive package for approved Arch runtime packages. | High-performance memory/collection primitives used by Arch. | Transitive module runtime only. | No direct module reference unless explicitly promoted. |
| `Microsoft.Extensions.ObjectPool` | Transitive package for approved Arch runtime packages. | Object pooling used by Arch package graph. | Transitive module runtime only. | No direct module reference unless explicitly promoted. |
| `ZeroAllocJobScheduler` | Transitive package for approved Arch runtime packages. | Scheduler support used by Arch.System. | Transitive module runtime only. | No direct module reference; source and binary checks deny `Schedulers.*` access. |
| Roslyn/source-generator transitive packages | Build graph only. | Analyzer/source-generator support. | Build/analyzer only. | Must be reachable from approved build packages only. |
| Safe BCL value APIs | shared, basegame, approved modules/mods. | Primitives, collections, spans/memory, math/numerics, dates/times, text, and diagnostics abstractions. | Shared contracts and module implementation. | Allowed namespace/API group only; no filesystem, network, reflection, process, environment, threading, or direct console writes. |
| Host-routed JSON/data parsing | basegame, approved modules/mods, tools. | Content parsing through approved host APIs. | Offline tools or bounded host API runtime path. | Module cannot open arbitrary paths; host supplies bounded streams/handles. |
| `LiteNetLib` | `octaryn-client`, `octaryn-server`. | Reliable UDP transport. | Host transport only. | Rejected in shared, basegame, modules, and mods. |
| `LiteEntitySystem` | `octaryn-client`, `octaryn-server`. | Host-side entity replication/synchronization backend. | Host implementation only. | Rejected in shared, basegame, modules, and mods; public entities/RPCs/SyncVars/replication declarations remain Octaryn API shapes. |

Denied to modules by default: `System.IO`, raw filesystem paths, `System.Net`, sockets, HTTP clients, `System.Diagnostics.Process`, unmanaged interop, unsafe native bridges, reflection/dynamic loading, runtime code generation, arbitrary threading/task scheduling, timers, custom worker pools, environment variables, direct host service discovery, direct console/stdout/stderr writes, and unlisted NuGet packages.

### Native Support Targets And Wrappers

Old native targets map to focused support or owner targets:

| Old target | Destination |
| --- | --- |
| `octaryn_engine_log` | `octaryn_native_logging`, used by client, server, tools, and focused support libraries. |
| `octaryn_engine_diagnostics` | `octaryn_native_diagnostics`, used by executables and tools that need crash reports. |
| `octaryn_engine_memory` | `octaryn_native_memory`; SDL coupling removed while porting. |
| `octaryn_engine_imgui_backend` | Client debug/tool UI only. |
| `octaryn_engine_shader_tool` | Root `tools/` shader compiler; generated shaders are client-owned assets. |
| `octaryn_engine_texture_atlas` | `octaryn-basegame/Tools/`; generated atlas assets are consumed by client. |
| `octaryn_managed_game` | `octaryn-basegame`. |
| `octaryn_engine_shader_assets` | `octaryn-client` asset build. |
| `octaryn_engine_runtime_assets` | `octaryn-client` packaging. |
| `octaryn_engine_runtime_bundle` | Replaced by `octaryn_client_bundle`; old name only in migration notes. |
| `octaryn_engine_runtime` | Split across client, server, shared, basegame, tools, and focused support targets. Do not recreate it. |

Native dependency wrappers:

| Wrapper | Allowed owners |
| --- | --- |
| `octaryn::deps::spdlog` | Native logging support, client, server, tools. |
| `octaryn::deps::cpptrace` | Native diagnostics support and executable crash paths. |
| `octaryn::deps::tracy` | Client, server, tools, and profiling-enabled support. |
| `octaryn::deps::mimalloc` | Native memory support only; consumers use the support target. |
| `octaryn::deps::taskflow` | Native jobs support below Octaryn scheduler policy; modules never see Taskflow. |
| `octaryn::deps::unordered_dense` | Owner-local implementation code that needs dense hash containers. |
| `octaryn::deps::eigen` | Shared pure math/value code or owner-local math; rendering-only math stays client-owned. |
| `octaryn::deps::glaze` | JSON metadata/manifests/tooling; shared contracts only if pure value-shape need is approved. |
| `octaryn::deps::sdl3` | Client only, except isolated tools that truly need SDL. |
| `octaryn::deps::sdl3_image` | Client UI/assets and asset import tools. |
| `octaryn::deps::sdl3_ttf` | Client UI and overlays only; not product UI framework. |
| ImGui-related wrappers | Client debug UI and tools only. |
| Shader tooling wrappers | Shader tooling only. |
| Asset import wrappers | Asset tools; client only for intentional runtime loading/optimization. |
| `octaryn::deps::openal` | Planned hidden spatial runtime audio backend under client. |
| `octaryn::deps::miniaudio` | Client audio helpers and tools unless benchmarks consolidate differently. |
| `octaryn::deps::zlib`, `lz4`, `zstd` | Server persistence, asset tools, and tooling caches as appropriate. |

## Validation Requirements

Before a module can activate, validate:

- manifest identity and version
- requested capabilities
- requested host APIs
- content declarations
- asset declarations
- component declarations
- system declarations
- read/write sets
- phase ownership
- package allowlist
- framework API allowlist
- native ABI if present
- no direct threading/task creation
- no direct filesystem/process/network/reflection/native interop; approved capabilities grant bounded Octaryn handles only
- replication and persistence schema compatibility
- multiplayer compatibility
- trust tier and signature/hash policy
- save schema and migration compatibility
- asset hashes and cooked payload consistency
- component schema ABI/layout identity
- replication schema compatibility
- world bounds invariants
- deterministic scheduler ordering
- save corruption recovery path
- performance budgets for chunk meshing, replication packing, save writes, and fluid/gas simulation

Validation contracts live in `octaryn-shared`. Validator implementation lives in `tools`. Activation gates and execution live in client/server hosts.

Validator backlog from the research report:

| Validator/probe | Enforces |
| --- | --- |
| Managed IL sandbox scan | No P/Invoke, denied framework namespaces, raw threading/timers/process/filesystem/network APIs, or dynamic loading. |
| Component schema ABI check | Arch/generated descriptors and native descriptors agree on field layout, version, serializer identity, authority, replication, and persistence policy. |
| Replication compatibility probe | Component replication annotations and network IDs remain stable or migrate explicitly. |
| Save migration replay validator | Old saves can open, migrate, resave, and preserve declared compatibility. |
| Content ID collision validator | Block, item, entity, tag, recipe, loot, UI, and asset IDs do not collide across modules. |
| Asset hash and manifest validator | Package manifests match cooked payloads, hashes, declared assets, and multiplayer compatibility metadata. |
| Shader reflection/material ABI validator | Cooked shaders match client material and render-contract expectations. |
| World bounds invariant validator | 512-height world model stays independent from chunk width/depth constants. |
| Deterministic scheduling probe | Read/write declarations, phase graph, barriers, and ordering produce stable results. |
| Save corruption recovery probe | Journals, checksums, backups, and recovery paths work under direct runtime checks. |
| Performance budget probes | Chunk meshing, replication packing, save writes, fluid/gas simulation, and UI layout stay within declared thresholds. |
| Trust/signature validator | Signed package policy, server-required mod list, content hashes, and client negotiation are coherent. |

If a failure can cause silent corruption, undefined privilege, broken multiplayer, or broken save compatibility, it deserves a validator before the feature is treated as complete.

Targeted validation paths for plan and structure work:

- `git diff --check -- README.md AGENTS.md docs .github`
- grep docs for stale `smoke`, `ctest`, old product naming, generic bucket names, and retired research wording.
- `python3 tools/validation/validate_cmake_target_inventory.py --build-dir build/debug-linux/cmake` when a configured build tree exists.
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_policy_separation`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_dependency_aliases`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_project_references`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_package_policy_sync`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_abi_contracts`
- `tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_owner_boundaries`

If a plan change claims current bundle/module behavior, also use the matching targeted owner validation:

- `octaryn_validate_module_manifest_probe`
- `octaryn_validate_module_layout`
- `octaryn_validate_basegame_block_catalog`
- `octaryn_validate_server_world_generation_probe`
- `octaryn_validate_client_server_app`
- `octaryn_validate_bundle_module_payload`

Do not use smoke tests or `ctest` as validation paths unless the user explicitly asks.
