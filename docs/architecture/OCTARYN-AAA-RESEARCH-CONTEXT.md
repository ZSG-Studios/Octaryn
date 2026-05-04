# Octaryn AAA Research Context

## Current Project Base

Repository root:

```text
/home/zacharyr/octaryn-workspace
```

Active owner roots:

```text
octaryn-client/
octaryn-server/
octaryn-shared/
octaryn-basegame/
tools/
cmake/
docs/
refrances/
old-architecture/
```

Generated output roots:

```text
build/<preset>/<owner>/
build/<preset>/deps/
build/dependencies/
logs/<owner>/
```

Primary language and build base:

- C++23 and C17 for native owner code.
- CMake 3.28+ for native/build orchestration.
- .NET 10 and latest C# for managed shared/basegame/client/server code.
- Linux/Arch first with Linux-hosted Windows cross builds.
- Public presets are expected to be `debug-linux`, `release-linux`, `debug-windows`, and `release-windows`.
- Validation should use targeted builds, direct runtime probes, owner launch probes, structure validators, profiling logs, or focused benchmarks.
- Do not use smoke tests or `ctest` as validation unless explicitly requested.

## Hard Architecture Rules

- Keep strict separation between `octaryn-client`, `octaryn-server`, `octaryn-shared`, `octaryn-basegame`, `tools`, and `cmake`.
- Do not create new top-level `engine/`, `octaryn-engine/`, generic `runtime/`, `common`, `helpers`, `misc`, or catch-all buckets.
- Treat `old-architecture/` as source material only.
- Port behavior into the correct owner with the smallest practical changes.
- Preserve behavior unless a boundary/API change is required.
- Keep client presentation/rendering out of server.
- Keep server authority/persistence/simulation out of client.
- Keep gameplay/content in basegame or another game module.
- Keep shared implementation-free and contract/API focused.
- Keep module/game/mod APIs explicit, capability-scoped, and deny-by-default.
- Keep build outputs under `build/<preset>/<owner>/`.
- Keep logs under `logs/<owner>/`.
- Do not expose raw backend internals to basegame, games, or mods.
- Do not expose broad service locators, native pointers, renderer handles, raw physics worlds, raw ECS storage, raw sockets, transport sessions, filesystem access, arbitrary threading, process control, or scheduler internals to modules.

## Owner Responsibilities

### `octaryn-client`

Owns presentation:

- windowing
- input collection and mapping
- rendering
- shaders
- GPU upload
- audio
- UI rendering
- overlays
- local prediction
- interpolation
- client host code
- screen-space UI execution
- world-space UI execution
- render-to-texture
- textured 3D UI quads/panels
- focus handling
- raycast input routing for world-space UI

Client must not own authoritative world edits, persistence authority, server simulation, or product game rules.

### `octaryn-server`

Owns authority:

- simulation
- validation
- persistence
- world saves
- server ticks
- replication
- transport hosting
- authoritative physics
- authoritative world edits
- server-side fluid/gas/block/entity simulation
- server-side module activation and validation

Server must not own GPU upload, render descriptors, shaders for presentation, product UI, audio, or client-only rendering.

### `octaryn-shared`

Owns clean API contracts and implementation-free value types:

- host interfaces
- tick contracts
- commands
- snapshots
- registries
- queries
- IDs
- positions
- world bounds contracts
- replication contracts
- component declarations
- system declarations
- module manifests
- dependency declarations
- content declarations
- asset declarations
- capability IDs
- API allowlists
- framework/package allowlists
- validation-facing shapes

Shared must not contain product gameplay policy, rendering implementation, persistence implementation, networking transport, physics backend implementation, or third-party backend types in public APIs.

### `octaryn-basegame`

Owns the default bundled game module:

- blocks
- items
- materials
- recipes
- tags
- loot
- feature and biome rules
- player rules
- interactions
- base content data
- basegame main menu
- pause menu
- inventory UI
- HUD
- world-space panels
- block/entity panels
- product-specific options
- high-level gameplay systems

Basegame is not privileged engine internals. It must use the same public API model as future games and mods.

### `tools`

Owns repo-wide developer operations:

- build orchestration
- profiling capture wrappers
- validation tools
- module inspection
- package policy validators
- shader compiler tools
- workspace UI/dev tools
- ABI checks
- packaging checks

Basegame-specific content tools belong under `octaryn-basegame/Tools/`, not root `tools/`.

### `cmake`

Owns build/dependency/toolchain policy:

- `cmake/Shared/`
- `cmake/Owners/`
- `cmake/Dependencies/`
- `cmake/Platforms/`
- `cmake/Toolchains/`

Do not mix platform detection into owner target definitions. Do not turn root `cmake/` into a dump for old monolithic modules.

## Current Core Host Baseline

The core host should boot into a minimal inspectable world:

- flying camera only
- no default player physics
- no default collision controller
- no default survival/avatar rules
- no product main menu or product UI
- flat blank terrain
- target world height is 512 blocks
- vertical world span should be centered around origin
- deterministic owner-routed build/log output

Basegame, game modules, or mods add:

- player movement rules
- physics bodies
- entity controllers
- world generation
- interactions
- inventory/items
- UI overlays
- main menu/front-end flow
- progression/game state

Existing 256-height constants or chunk-edge-derived height constants are migration debt. Research should plan a clean world constants model where chunk width/depth and world height are separate concepts.

## Current ECS And API Direction

Octaryn should be ECS-based for:

- entities
- blocks
- items
- UI state
- global game state
- input actions
- world interactions
- fluids
- gases
- machines
- projectiles
- abilities
- rules
- content systems

The target author experience:

- Define the thing.
- Attach components.
- Write allowed logic through explicit APIs.
- Declare replication and persistence intent.
- Let the host-owned backend ECS handle storage, scheduling, networking, persistence, validation, and presentation handoff.

Native C++ should own:

- fast ECS storage and query execution
- scheduling
- worker execution
- native simulation kernels
- networking packers
- persistence packers
- world interaction pipelines
- physics backend integration
- high-throughput validation and packaging paths

C# should own:

- ergonomic public API declarations
- basegame gameplay logic where appropriate
- game module and mod authoring
- managed system definitions
- content registration
- component declarations
- module manifests
- capability declarations

The shared API should expose contracts, not storage. Modules should never access raw ECS storage or backend implementation types.

## UI Direction

Octaryn needs first-class UI API support for:

- screen-space UI
- world-space UI
- render-to-texture UI
- textured 3D quads and panels
- HUD
- menus
- inventory
- block/entity panels
- nameplates
- diegetic controls
- tool/editor/debug UI where allowed
- pointer, keyboard, controller, and gamepad routing
- focus and capture
- raycast-to-UI mapping
- layout, styling, fonts, text shaping, localization, accessibility, animation, transitions, and input actions

Client renderer owns execution and presentation. Basegame/modules declare UI models, surfaces, anchors, bindings, actions, and styling through approved APIs.

Core/client-owned UI is limited to debug, diagnostics, profiler, validation, editor/developer, and emergency host surfaces. Product UI belongs in basegame or another game module.

## DDGI And Lighting Hold

DDGI, skylight propagation, lighting architecture, and old CPU skylight behavior are on hold until a dedicated user-approved lighting plan exists.

Research may include a future placeholder for DDGI/lighting planning, but do not treat lighting as an active implementation slice. Do not recommend immediate DDGI/skylight implementation work in this research output.

## Active Project Files And Structure To Consider

Managed project files:

```text
octaryn-shared/Octaryn.Shared.csproj
octaryn-basegame/Octaryn.Basegame.csproj
octaryn-client/Octaryn.Client.csproj
octaryn-server/Octaryn.Server.csproj
```

Native/build project files:

```text
CMakeLists.txt
octaryn-client/CMakeLists.txt
cmake/Shared/*.cmake
cmake/Owners/*.cmake
cmake/Dependencies/*.cmake
cmake/Platforms/Linux/*.cmake
cmake/Platforms/Windows/*.cmake
cmake/Toolchains/Linux/clang.cmake
cmake/Toolchains/Windows/clang.cmake
```

Current source roots include:

```text
octaryn-shared/Source/ApiExposure/
octaryn-shared/Source/FrameworkAllowlist/
octaryn-shared/Source/GameModules/
octaryn-shared/Source/Host/
octaryn-shared/Source/ModuleSandbox/
octaryn-shared/Source/Networking/
octaryn-shared/Source/Time/
octaryn-shared/Source/World/

octaryn-basegame/Source/Managed/
octaryn-basegame/Source/Module/
octaryn-basegame/Data/
octaryn-basegame/Assets/
octaryn-basegame/Tools/

octaryn-client/Source/ClientHost/
octaryn-client/Source/Managed/
octaryn-client/Source/WorldPresentation/
octaryn-client/Shaders/

octaryn-server/Source/Managed/
octaryn-server/Source/Tick/
octaryn-server/Source/Validation/
octaryn-server/Source/Networking/
octaryn-server/Source/Persistence/
octaryn-server/Source/Physics/
octaryn-server/Source/Simulation/

tools/validation/
tools/package-policy/
tools/build/
tools/profiling/
tools/ui/
tools/Source/ShaderCompiler/
```

Current docs to align with:

```text
AGENTS.md
docs/architecture/octaryn-appendix.md
docs/architecture/octaryn-master-plan.md
docs/architecture/octaryn-master-plan-api.md
docs/architecture/octaryn-master-plan-gameplay.md
docs/architecture/octaryn-master-plan-build-and-validation.md
docs/architecture/octaryn-master-plan-roadmap.md
```

## Current Build And Validation Targets

Root aggregate target:

```text
octaryn_all
```

Current owner targets include:

```text
octaryn_shared
octaryn_basegame
octaryn_server
octaryn_server_native
octaryn_server_bundle
octaryn_client_native
octaryn_client_bundle
octaryn_tools
```

Current validation aggregate:

```text
octaryn_validate_all
```

Known validation targets include:

```text
octaryn_validate_cmake_targets
octaryn_validate_cmake_policy_separation
octaryn_validate_cmake_dependency_aliases
octaryn_validate_package_policy_sync
octaryn_validate_project_references
octaryn_validate_module_manifest_packages
octaryn_validate_module_manifest_files
octaryn_validate_module_manifest_probe
octaryn_validate_bundle_module_payload
octaryn_validate_module_source_api
octaryn_validate_module_binary_sandbox
octaryn_validate_module_layout
octaryn_validate_basegame_block_catalog
octaryn_validate_dotnet_package_assets
octaryn_validate_native_abi_contracts
octaryn_validate_native_owner_boundaries
octaryn_validate_native_archive_format
octaryn_validate_dotnet_owners
octaryn_validate_scheduler_contract
octaryn_validate_scheduler_probe
octaryn_validate_world_time_probe
octaryn_validate_server_world_blocks_probe
octaryn_validate_basegame_player_probe
octaryn_validate_basegame_interaction_probe
octaryn_validate_client_world_presentation_probe
octaryn_validate_owner_module_validation_probe
octaryn_validate_hostfxr_bridge_exports
octaryn_validate_owner_launch_probes
```

Research should propose additional validators only when they enforce a real boundary, API contract, capability rule, dependency rule, performance invariant, serialization format, or module activation requirement.
