# Octaryn Appendix: Build And Libraries

## Support Libraries

Do not create a generic runtime root. The old native tree already has focused support libs for logging, diagnostics, memory, jobs, dependency wrappers, profiling, and shader tooling. During the port, keep those as small build targets owned by the layer that needs them.

Agent workers must check this library catalog before implementing any feature. If a listed library matches the job, use it through the owner that is allowed to depend on it. Do not hand-roll replacement systems, add parallel dependencies, or move a library across ownership boundaries without updating this plan and the migration map.

Native support landing zones:

- `octaryn-client/Source/Host/`
- `octaryn-client/Source/FrameLoop/`
- `octaryn-client/Source/HostBridge/`
- `octaryn-client/Source/Libraries/<ExactLibraryName>/` for client-local native support.
- `octaryn-server/Source/Tick/`
- `octaryn-server/Source/Simulation/`
- `octaryn-server/Source/Libraries/<ExactLibraryName>/` for server-local native support.
- `octaryn-shared/Source/Diagnostics/`
- `octaryn-basegame/Source/Libraries/<ExactLibraryName>/` only for basegame-local native support that does not expose host internals.
- `tools/<ExactToolName>/` for repo-wide developer tools that are not owned by a game/content package.
- focused native support targets such as `octaryn_native_logging`, `octaryn_native_diagnostics`, `octaryn_native_memory`, `octaryn_native_profiling`, and `octaryn_native_jobs`.

Port source candidates:

- `old-architecture/source/core/log.*` -> focused diagnostics/logging lib used by client/server.
- `old-architecture/source/core/memory_mimalloc.*` -> native build support linked where needed.
- `old-architecture/source/core/crash_diagnostics.*` -> diagnostics support, not a product root.
- `old-architecture/source/runtime/jobs/` -> client/server job support or server simulation scheduler depending usage.
- native managed-host bridge pieces -> client host or shared contracts after renaming away from engine API names.

## CMake And Platform Build Architecture

Root `cmake/` owns only new-architecture build policy. Old CMake files stay under `old-architecture/cmake/` until intentionally ported into the structure below. Do not move old CMake modules wholesale; split them by responsibility first.

The tree below is the required target structure. In the current workspace these paths are placeholders unless the named `.cmake` file exists. Do not describe Windows, Linux, owner target, dependency, or root preset support as implemented until the concrete module and a targeted configure check exist.

```text
cmake/
  Shared/
    ProjectDefaults.cmake
    CompilerWarnings.cmake
    BuildOutputs.cmake
    OwnerBuildLayout.cmake
  Owners/
    DotNetOwner.cmake
    NativeOwner.cmake
    ClientTargets.cmake
    ServerTargets.cmake
    SharedTargets.cmake
    BasegameTargets.cmake
    ToolTargets.cmake
  Dependencies/
    DependencyPolicy.cmake
    DotNetHosting.cmake
    SourceDependencyCache.cmake
    NativeDependencyAliases.cmake
    ClientDependencies.cmake
    ToolDependencies.cmake
  Platforms/
    PlatformDispatch.cmake
    Windows/
      WindowsPlatform.cmake
    Linux/
      LinuxPlatform.cmake
      ArchFamily.cmake
      DebianFamily.cmake
      FedoraFamily.cmake
      SuseFamily.cmake
  Toolchains/
    Windows/
      clang.cmake
    Linux/
      clang.cmake
```

Layer responsibilities:

- `cmake/Shared/` owns repo-wide CMake defaults: C/C++ standards, warning policy, output layout, build/log owner paths, shared helper functions, and naming rules.
- `cmake/Owners/` owns target construction for client, server, shared contracts, basegame assets/modules, and tools. Owner modules may call shared helpers and dependency aliases, but must not contain platform detection.
- `cmake/Dependencies/` owns dependency wrappers and allowed dependency aliases. Dependencies must be grouped by real owner need; do not recreate one old global dependency bag.
- `cmake/Platforms/` owns host platform facts and distro-family policy: Windows, Linux family package hints, and platform capability checks.
- `cmake/Toolchains/` owns cross/native compiler toolchain files only. Toolchain files set compilers, sysroots, target triples, find-root behavior, and platform knobs; they must not create Octaryn targets or fetch dependencies.
- `tools/build/` owns new developer-facing build commands that select presets/toolchains and write to `build/<preset>/<owner>/` and `logs/<owner>/`.

Platform rules:

- Windows cross-builds from Linux use the explicit Windows Clang toolchain file under `cmake/Toolchains/Windows/clang.cmake`. LLVM MinGW is the implementation behind that toolchain, not a public platform folder or preset name.
- Linux-hosted builds are Clang-only. Public presets are exactly `debug-linux`, `release-linux`, `debug-windows`, and `release-windows`.
- Cross-platform builds are designed to run from Linux/Arch first. Active Podman build wrappers use the Linux-hosted toolchain environment for Linux and Windows targets instead of introducing separate host-specific build layouts; expand those wrappers in place when platform coverage grows.
- Linux policy is split by distro family only when real package/tool behavior differs. Start with Arch, Debian, Fedora, and Suse/openSUSE because the old dependency installer already has distinct package-manager logic for those families.
- Platform modules report capabilities; owner targets decide whether to use those capabilities. Platform modules must not own gameplay, rendering, server, basegame, or module-sandbox behavior.

Port map for old CMake:

- `old-architecture/cmake/BuildLayout.cmake` -> `cmake/Shared/OwnerBuildLayout.cmake`, after renaming away from engine product names and enforcing `build/<preset>/<owner>/` and `logs/<owner>/`.
- Old dependency cache paths such as `build/shared/deps/<bucket>` and `logs/deps/<bucket>` -> shared dependency sources/downloads under `build/dependencies/`, with preset-specific dependency build trees and population stamps under `build/<preset>/deps/`.
- `old-architecture/cmake/ProjectOptions.cmake` -> `cmake/Shared/ProjectDefaults.cmake` plus owner-specific options in `cmake/Owners/`.
- `old-architecture/cmake/Dependencies.cmake` -> `cmake/Dependencies/`, split by dependency policy, alias creation, and owner-specific dependency groups.
- `old-architecture/cmake/CPM.cmake` -> `cmake/Dependencies/` only if CPM remains the selected dependency mechanism.
- `old-architecture/cmake/toolchains/windows-x64.cmake` -> `cmake/Toolchains/Windows/clang.cmake` only as a Windows Clang cross toolchain; GCC-based MinGW is not an active lane.
- `old-architecture/CMakePresets.json` -> new root presets only after the owner/platform/toolchain split exists.
- `old-architecture/tools/build/configure.sh`, `cmake_build.sh`, `build_all.sh`, and repair/install helpers -> `tools/build/` only after they select new owner presets, use the new platform/toolchain modules, and write outputs to `build/<preset>/<owner>/`, `build/<preset>/deps/`, `build/dependencies/`, `logs/<owner>/`, or `logs/build/`.
- `old-architecture/tools/build/tracy_capture.sh` -> focused profiling wrappers under root `tools/` only after logs are moved away from old `logs/engine_control`, `logs/tracy`, and `logs/octaryn-engine` paths. Old RenderDoc helpers stay in `old-architecture/`; RenderDoc is handled by external developer installs.
- Old architecture scripts that remain under `old-architecture/` are source material only and are not accepted validation paths until intentionally ported to the active preset layout.

Validation for CMake changes:

- For structure-only CMake work, run `validate_cmake_target_inventory.py`; it verifies active target names, required owner/platform/dependency files, and absence of old generic product CMake paths.
- For build policy changes, configure the smallest owner target that uses the changed policy.
- For platform/toolchain changes, run targeted configure checks for the affected platform or toolchain when the host has that compiler/sysroot installed.
- Active configure presets are exactly `debug-linux`, `release-linux`, `debug-windows`, and `release-windows`. Linux native targeting is Clang-only in active lanes; GCC is not an active preset lane. Windows targets are cross-built inside the Linux/Arch builder with the Windows Clang toolchain; native Windows developer builds are not an active lane. Windows Clang configure may disable hostfxr bridge/probe targets when target-compatible .NET native hosting assets are unavailable, but Linux host validation must still build and run those bridge/probe targets.
- Do not validate CMake work with smoke tests or `ctest` unless explicitly requested.

## Library Catalog

Use these libraries for the cases they are intended for. Keep wrappers focused and owner-scoped; old `octaryn_engine_*` targets are source-material names only and must not be recreated in the active graph.

### Native Support Targets

| Old target | Purpose | Destination |
| --- | --- | --- |
| `octaryn_engine_log` | Native logging through `spdlog`. | `octaryn_native_logging`, used by client, server, tools, and support libraries that need logs. |
| `octaryn_engine_diagnostics` | Crash diagnostics and stack traces. | `octaryn_native_diagnostics`, used by executables and tools that need crash reports. |
| `octaryn_engine_memory` | Process allocator setup through `mimalloc`. | `octaryn_native_memory`; SDL coupling removed while porting. |
| `octaryn_engine_imgui_backend` | Dear ImGui backend glue for SDL3 and SDL GPU. | Client UI/debug UI only. |
| `octaryn_engine_shader_tool` | GLSL compilation, reflection, validation, and SPIR-V/MSL asset generation. | Root `tools/` shader compiler; generated shaders are client-owned assets. |
| `octaryn_engine_texture_atlas` | Builds block/material texture atlases from basegame content and packs. | `octaryn-basegame/Tools/`; generated atlas assets are consumed by the client. |
| `octaryn_managed_game` | Publishes the C# basegame assembly. | `octaryn-basegame`. |
| `octaryn_engine_shader_assets` | Generated shader asset target. | `octaryn-client` asset build. |
| `octaryn_engine_runtime_assets` | Copies generated assets into a runnable client bundle. | `octaryn-client` packaging. |
| `octaryn_engine_runtime_bundle` | Old monolithic runtime bundle. | Replaced by the active `octaryn_client_bundle`; keep the old name only in migration notes. |
| `octaryn_engine_runtime` | Old monolithic native executable. | Split across client, server, shared, high-level basegame mechanics/content, tools, and support targets. Do not recreate it under a new name. |

### Native Dependency Wrappers

| Wrapper | Purpose | Allowed owners |
| --- | --- | --- |
| `octaryn::deps::spdlog` | Fast structured/native logging. | Native logging support, client, server, tools. |
| `octaryn::deps::cpptrace` | Stack traces for crash diagnostics. | Native diagnostics support and executable crash paths. |
| `octaryn::deps::tracy` | Profiling instrumentation. | Client, server, tools, and native support when profiling is enabled. |
| `octaryn::deps::mimalloc` | Allocator backend. | Native memory support only; consumers use the support target. |
| `octaryn::deps::taskflow` | Job execution substrate. | Native jobs support below Octaryn-owned scheduling policy; modules never see Taskflow types or task graphs. |
| `octaryn::deps::unordered_dense` | High-performance hash maps and sets. | Owner-local implementation code that needs dense hash containers. |
| `octaryn::deps::eigen` | Math and linear algebra. | Shared pure math/value code or owner-local math; rendering-only math stays client-owned. |
| `octaryn::deps::glaze` | JSON and metadata serialization. | Shared contracts when pure, client settings persistence, server persistence, basegame content tools, root tools. |
| `octaryn::deps::sdl3` | Windowing, input, SDL GPU, platform services, timers. | Client only, except isolated tool use when a tool truly needs SDL. |
| `octaryn::deps::sdl3_image` | Image loading. | Client UI/assets and asset import tools. |
| `octaryn::deps::sdl3_ttf` | Text rendering and shaping/fallback support. | Client UI and overlays only; not a product UI framework by itself. |
| `octaryn::deps::imgui` | Immediate-mode runtime/debug UI. | Client UI/debug UI and tools. |
| `octaryn::deps::implot` | 2D plots and telemetry panels. | Client debug UI and tools. |
| `octaryn::deps::implot3d` | 3D plots and debug visualization. | Client debug UI and tools. |
| `octaryn::deps::imgui_node_editor` | Node graph editor UI. | Tools first; client only for explicit debug/editor UI. |
| `octaryn::deps::imguizmo` | Transform gizmos. | Tools first; client only for explicit debug/editor UI. |
| `octaryn::deps::imanim` | ImGui animation/editor widgets. | Tools first; do not ship in core client unless a real client feature uses it. |
| `octaryn::deps::imfiledialog` | ImGui file dialogs. | Tools or explicit client debug/editor UI. |
| `octaryn::deps::shaderc` | GLSL to SPIR-V compilation. | Shader tooling only. |
| `octaryn::deps::shadercross` | SDL shader cross-compilation. | Shader tooling only. |
| `octaryn::deps::spirv_tools` | SPIR-V validation and optimization. | Shader tooling only. |
| `octaryn::deps::spirv_cross` | Shader reflection and cross-compilation. | Shader tooling only. |
| `octaryn::deps::fastgltf` | glTF import/loading. | Asset import tools; client only if runtime glTF loading is intentionally added. |
| `octaryn::deps::ktx` | KTX texture containers and GPU texture pipeline. | Asset tools and intentional client texture loading. |
| `octaryn::deps::meshoptimizer` | Mesh optimization and import processing. | Asset tools; client only for intentional runtime optimization. |
| `octaryn::deps::ozz_animation` | Skeletal animation runtime. | Client animation runtime; basegame may own animation content data. |
| `octaryn::deps::openal` | Planned hidden spatial runtime audio backend. | Client audio; do not expose OpenAL types to modules. |
| `octaryn::deps::miniaudio` | Audio decode, streaming, helper, or tool roles unless benchmarks justify runtime consolidation the other way. | Client audio helpers and tools; do not keep as an equal first-class runtime backend indefinitely. |
| `octaryn::deps::zlib` | Compression. | Server persistence, asset tools, and support wrappers. |
| `octaryn::deps::lz4` | Fast save/cache compression. | Server persistence and tooling caches. |
| `octaryn::deps::zstd` | Strong save/cache compression. | Server persistence and tooling caches. |

Planned dependency decisions not yet implemented as active CMake coverage:

| Candidate | Decision | Owner boundary |
| --- | --- | --- |
| Jolt | First physics backend candidate. | Hidden under `octaryn-server` authority and client prediction/presentation wrappers; modules see only Octaryn physics APIs. |
| Yoga | First retained-UI layout solver candidate. | Hidden under `octaryn-client` UI execution; modules see only Octaryn UI declarations. |
| RmlUi | Deferred candidate only if a user-approved UI authoring plan chooses markup-style product UI. | No active dependency or API surface; must stay behind Octaryn UI contracts if adopted later. |
| FlatBuffers | Optional control-plane schema candidate. | Not the primary voxel/chunk/entity save format. |
| Recast/Detour | Later navigation candidate. | Defer until world, physics, persistence, and tool spines are stable. |

### Managed Packages

| Package | Purpose | Allowed owners |
| --- | --- | --- |
| `Arch` | Intended managed ECS for gameplay, basegame, modules, mods, and approved owner-local managed worlds. | `octaryn-basegame`; client/server only for private owner-local state, never `octaryn-shared`. |
| `Arch.System` | Intended managed ECS system authoring/execution support, driven by Octaryn host scheduling declarations. | Same as `Arch`; do not expose scheduler internals or raw threading to modules. |
| `Arch.System.SourceGenerator` | Compile-time support for `Arch.System`. | Only projects that directly define Arch systems; keep analyzer/private. |
| `Arch.EventBus` | Gameplay event bus for Arch-style systems. | `octaryn-basegame` or private owner-local systems, never shared contracts. |
| `Arch.Relationships` | ECS entity relationship helpers. | `octaryn-basegame` or private owner-local systems, never shared contracts. |
| `LiteNetLib` | Reliable UDP transport. | `octaryn-client` and `octaryn-server` transport implementation only. |
| `LiteEntitySystem` | Host-side entity replication/synchronization backend behind Octaryn networking contracts. | `octaryn-client` and `octaryn-server` only; never shared/basegame/module public APIs. |

`LiteNetLib` and `LiteEntitySystem` stay centrally pinned for host networking work, but they are not module permissions. Basegame must talk through shared command, snapshot, registry, interaction, feature, query, and scheduling contracts rather than transport packages, raw threads, or client/server internals.

Host-owned C# ECS or networking packages may be driven from C/C++ through narrow client/server bridge contracts when that is the best owner route. That does not make those packages module-facing or weaken the module sandbox.

Sandboxed game modules and mods must not reference host-only packages or unlisted NuGet packages. `Directory.Packages.props` is only the central version pin file; it is not permission to use a package from module code.

## Old Architecture Tooling

Old build helpers, old CMake modules, old desktop helper tools, and old profiling wrappers stay under `old-architecture/` until they are intentionally ported.

The old atlas builder is basegame-specific content tooling and belongs under `octaryn-basegame/Tools/AtlasBuilder/` or another focused basegame tool folder. Keep a one-file script only if it stays small and deliberately scoped.

Active root `cmake/` and `tools/` are reserved for new architecture support only. Generated outputs should be owner-partitioned under `build/<preset>/<owner>/` and `logs/<owner>/`.

Bundle composition rules:

- `octaryn_client_bundle` is the graphical client package. It must include validated client assets, shared contracts, basegame/module payloads, and the version-matched `server/` payload required for local singleplayer worlds.
- `octaryn_server_bundle` is the dedicated headless server package. It must include server authority, shared contracts, basegame/module payloads, and server-side validation without any client presentation payload.
- Server files copied into the client bundle must originate from server-owned build outputs. Do not compile those server files as client-owned implementation.
- Future bundle validators must reject missing `server/` payloads in the client bundle and reject client rendering/window/audio/UI payloads in the dedicated server bundle.

## Build Target Names

These are CMake/build target names, not necessarily shipped executable or folder names. Shipped local singleplayer naming stays `client_server_app` with a bundled `server/` payload.

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
octaryn_validate_scheduler_contract
octaryn_validate_scheduler_probe
octaryn_validate_world_time_probe
octaryn_validate_owner_module_validation_probe
octaryn_validate_server_world_blocks_probe
octaryn_validate_server_world_generation_probe
octaryn_validate_basegame_player_probe
octaryn_validate_basegame_interaction_probe
octaryn_validate_client_world_presentation_probe
octaryn_validate_hostfxr_bridge_exports
octaryn_validate_owner_launch_probes
octaryn_run_client_launch_probe
octaryn_run_server_launch_probe
```

Internal dependency targets such as `octaryn_dotnet_hosting` and `octaryn_native_threads` are implementation details for owner CMake modules. They are not public build targets, but they must stay under `cmake/Dependencies/` and remain out of old-architecture target wiring.

Planned focused targets, added only when their implementation exists:

```text
octaryn_client_assets
octaryn_basegame_assets
octaryn_module_validator
octaryn_module_sandbox_contracts
```

## Validation

- Do not use smoke tests unless explicitly requested.
- Do not run `ctest` unless explicitly requested.
- Use direct runtime launches, targeted benchmarks, Tracy captures, focused logs, and external GPU captures when a developer has a local capture tool installed.
- For reference-parity work, inspect Minecraft, Iris, and Complementary Reimagined before implementing behavior.
- For architecture-only structure work, verify file tree shape, empty-file/placeholders, owner partitioning, and absence of stale old product names in active roots.
- For every touched old-architecture file, verify the source-to-destination map or a removal reason is recorded.
- For module/API/package work, verify no unapproved package, transitive package, framework API group, unsafe/native bridge, direct console write, reflection, filesystem, network, process, threading, or dynamic loading access was introduced.
- For CMake work, verify old dependency/log paths are not recreated in active roots and targeted configure checks cover the changed owner/platform/toolchain when practical.

## Phase Order

1. Create blank owner structure and migration maps.
2. Rename managed API away from `Octaryn.Engine.Api` into shared contracts and client/server host contracts.
3. Define shared game-module manifest, API exposure, package allowlist, compatibility, validation, registry, command, snapshot, and query contracts.
4. Define the host scheduling contract: main thread, coordinator thread, scalable worker pool with at least two workers, scheduled system declarations, and thread-safety rules for all computation and gameplay logic.
5. Create the new CMake split: shared build policy, owner targets, dependency aliases, platform modules, and toolchains.
6. Split build target names away from `octaryn_engine_*`.
7. Port shared value contracts.
8. Port server-authoritative world and persistence behind shared APIs and coordinator-scheduled worker jobs.
9. Port client presentation, windowing, rendering, shaders, and uploads behind shared APIs, keeping computation on worker jobs and presentation handoff on client-owned main-thread paths.
10. Port basegame as the first validated game module with high-level content and gameplay rules only, scheduled through host APIs.
11. Wire client-server transport through shared message contracts.
12. Validate module loading, package allowlist checks, API exposure checks, scheduler/thread-safety checks, CMake platform/toolchain checks, compatibility checks, and runtime/profiling paths.

For the current non-lighting old-architecture port loop, use the queue in `docs/architecture/octaryn-appendix.md` to choose phase 8-10 slices. Start with bundle-root data/asset discovery, then real atlas upload, then player movement/raycast/persistence before dynamic fluids and larger inventory/UI work.

Lighting hold:

- DDGI is the intended future lighting direction, but it is not active work until the user provides a dedicated plan.
- Do not port old CPU skylight propagation as the lighting implementation path.
- Do not add interim server lighting contracts, DDGI probes, or client lighting rewrites while this hold is active.
- Treat old skylight files as reference/source material only for future planning, not as the next port target.
