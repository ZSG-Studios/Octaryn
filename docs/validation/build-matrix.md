# Build Matrix

Last updated during the AAA port loop.

## Canonical Commands

Launch the workspace UI through the Linux launcher so the host setup is checked before the PySide app starts:

```sh
tools/run_workspace_ui.sh
```

The launcher validates native Python/PySide6 and Podman, builds the Arch builder image, verifies the workspace mount, then starts `tools/ui/workspace_control_app.py` with native Python. Host setup installs only Linux launcher requirements: Git, Python, PySide6, Podman, and Podman runtime support. Compilers, CMake, Ninja, .NET, LLVM MinGW, graphics/audio development libraries, and dependency build tools live inside the Arch builder image. The UI sends configure/build/validate actions through `tools/build/podman_build.sh`; CMake still runs through the existing CMake helpers inside the Arch builder. Windows is a Linux/Podman cross-build target only.

```sh
tools/build/podman_build.sh configure debug-linux
tools/build/podman_build.sh configure release-linux
tools/build/podman_build.sh configure debug-windows
tools/build/podman_build.sh configure release-windows
tools/build/podman_build.sh build debug-linux --target octaryn_all
tools/build/podman_build.sh build release-linux --target octaryn_all
tools/build/podman_build.sh build debug-windows --target octaryn_all
tools/build/podman_build.sh build release-windows --target octaryn_all
tools/build/podman_build.sh validate debug-linux
tools/build/podman_build.sh validate release-linux
tools/build/podman_build.sh validate debug-windows
tools/build/podman_build.sh validate release-windows
tools/build/podman_build.sh build-all
tools/build/podman_build.sh build debug-linux --target octaryn_debug_tools
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh configure debug-linux
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh configure release-linux
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh configure debug-windows
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh configure release-windows
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh build debug-linux --target octaryn_validate_native_archive_format
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh build release-linux --target octaryn_validate_native_archive_format
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh build debug-windows --target octaryn_validate_native_archive_format
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh build release-windows --target octaryn_validate_native_archive_format
```

Linux ARM64 sysroot setup is workspace-managed:

```sh
tools/build/workspace_bootstrap.sh linux-arm64
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh configure debug-linux
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh configure release-linux
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh build debug-linux --target octaryn_validate_native_archive_format
OCTARYN_TARGET_ARCH=arm64 tools/build/podman_build.sh build release-linux --target octaryn_validate_native_archive_format
```

## Expanded Debug Validators

```sh
tools/build/cmake_build.sh debug-linux --target octaryn_client_managed_bridge
tools/build/cmake_build.sh debug-linux --target octaryn_client_camera_matrix
tools/build/cmake_build.sh debug-linux --target octaryn_client_render_distance
tools/build/cmake_build.sh debug-linux --target octaryn_server_managed_bridge
tools/build/cmake_build.sh debug-linux --target octaryn_basegame_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_server_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_client_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_run_client_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_run_client_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_run_server_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_targets
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_policy_separation
tools/build/cmake_build.sh debug-linux --target octaryn_validate_cmake_dependency_aliases
tools/build/cmake_build.sh debug-linux --target octaryn_validate_package_policy_sync
tools/build/cmake_build.sh debug-linux --target octaryn_validate_project_references
tools/build/cmake_build.sh debug-linux --target octaryn_validate_module_manifest_packages
tools/build/cmake_build.sh debug-linux --target octaryn_validate_module_manifest_files
tools/build/cmake_build.sh debug-linux --target octaryn_validate_module_manifest_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_bundle_module_payload
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_server_app
tools/build/cmake_build.sh debug-linux --target octaryn_client_server_app_launch_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_module_source_api
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_shader_bundle
tools/build/cmake_build.sh debug-linux --target octaryn_validate_module_binary_sandbox
tools/build/cmake_build.sh debug-linux --target octaryn_validate_module_layout
tools/build/cmake_build.sh debug-linux --target octaryn_validate_basegame_block_catalog
tools/build/cmake_build.sh debug-linux --target octaryn_validate_basegame_worldgen_content
tools/build/cmake_build.sh debug-linux --target octaryn_validate_dotnet_package_assets
tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_abi_contracts
tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_owner_boundaries
tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_archive_format
tools/build/cmake_build.sh debug-linux --target octaryn_validate_native_jobs_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_chunk_mesh_plan_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_client_empty_world_mesh_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_host_policy_native_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_dotnet_owners
tools/build/cmake_build.sh debug-linux --target octaryn_validate_world_time_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_world_time_native_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_authority_tick_native_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_block_store_native_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_player_simulation_native_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_terrain_generation_native_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_world_persistence_native_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_persistence_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_world_blocks_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_server_world_generation_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_basegame_player_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_basegame_interaction_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_owner_module_validation_probe
tools/build/cmake_build.sh debug-linux --target octaryn_validate_hostfxr_bridge_exports
tools/build/cmake_build.sh debug-linux --target octaryn_validate_owner_launch_probes
tools/build/cmake_build.sh debug-linux --target octaryn_validate_all
python3 tools/validation/validate_all_project_reference_boundaries.py --repo-root .
dotnet run --project tools/validation/Octaryn.ModuleApiProbe/Octaryn.ModuleApiProbe.csproj --configuration Debug -- octaryn-basegame
dotnet run --project tools/validation/Octaryn.ModuleBinarySandboxProbe/Octaryn.ModuleBinarySandboxProbe.csproj --configuration Debug -- --assembly build/debug-linux/basegame/managed/Octaryn.Basegame.dll --assets-file build/debug-linux/basegame/managed-obj/project.assets.json
dotnet run --project tools/validation/Octaryn.ModuleManifestProbe/Octaryn.ModuleManifestProbe.csproj --configuration Debug -- octaryn-basegame --dump-manifest build/debug-linux/basegame/generated/octaryn.basegame.manifest.json
python3 -m json.tool octaryn-basegame/Data/Module/octaryn.basegame.module.json >/dev/null
dotnet run --project tools/validation/Octaryn.OwnerModuleValidationProbe/Octaryn.OwnerModuleValidationProbe.csproj --configuration Debug
OCTARYN_SERVER_WORLD_TIME_LIBRARY=build/debug-linux/server/native/lib/liboctaryn_server_world_time.so OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY=build/debug-linux/server/native/lib/liboctaryn_server_world_persistence.so dotnet run --project tools/validation/Octaryn.WorldTimeProbe/Octaryn.WorldTimeProbe.csproj --configuration Debug
dotnet run --project tools/validation/Octaryn.ServerPersistenceProbe/Octaryn.ServerPersistenceProbe.csproj --configuration Debug
dotnet run --project tools/validation/Octaryn.ServerWorldBlocksProbe/Octaryn.ServerWorldBlocksProbe.csproj --configuration Debug
dotnet run --project tools/validation/Octaryn.ServerWorldGenerationProbe/Octaryn.ServerWorldGenerationProbe.csproj --configuration Debug
dotnet run --project tools/validation/Octaryn.BasegamePlayerProbe/Octaryn.BasegamePlayerProbe.csproj --configuration Debug
dotnet run --project tools/validation/Octaryn.BasegameInteractionProbe/Octaryn.BasegameInteractionProbe.csproj --configuration Debug
python3 tools/validation/validate_module_layout.py --module-root octaryn-basegame
python3 tools/validation/validate_basegame_block_catalog.py --catalog octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json --generated-source octaryn-basegame/Source/Content/Blocks/BlockCatalog.cs --atlas-color octaryn-basegame/Assets/Atlases/basegame-color.png --atlas-normal octaryn-basegame/Assets/Atlases/basegame-normal.png --atlas-specular octaryn-basegame/Assets/Atlases/basegame-specular.png --animation-atlas octaryn-basegame/Assets/Atlases/basegame-animation.png --animation-manifest octaryn-basegame/Assets/Atlases/basegame-animation.txt
python3 tools/validation/validate_basegame_worldgen_content.py --block-catalog octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json --biomes octaryn-basegame/Data/Biomes/octaryn.basegame.biomes.json --features octaryn-basegame/Data/Features/octaryn.basegame.features.json --terrain-rule octaryn-basegame/Data/Rules/octaryn.basegame.rule.terrain_generation.json
python3 tools/validation/validate_dotnet_package_assets.py --assets-file build/debug-linux/client/managed-obj/project.assets.json --owner client
python3 tools/validation/validate_dotnet_package_assets.py --assets-file build/debug-linux/server/managed-obj/project.assets.json --owner server
python3 tools/validation/validate_dotnet_package_assets.py --assets-file build/debug-linux/basegame/managed-obj/project.assets.json --owner basegame
python3 tools/validation/validate_module_manifest_packages.py --module-root octaryn-basegame --project-file octaryn-basegame/Octaryn.Basegame.csproj --manifest-json build/debug-linux/basegame/generated/octaryn.basegame.manifest.json
python3 tools/validation/validate_module_manifest_files.py --module-root octaryn-basegame --manifest-json build/debug-linux/basegame/generated/octaryn.basegame.manifest.json
python3 tools/validation/validate_bundle_module_payload.py --bundle-root build/debug-linux/client/bundle --module-id octaryn.basegame --expected-manifest build/debug-linux/basegame/generated/octaryn.basegame.manifest.json
python3 tools/validation/validate_bundle_module_payload.py --bundle-root build/debug-linux/server/bundle --module-id octaryn.basegame --expected-manifest build/debug-linux/basegame/generated/octaryn.basegame.manifest.json
python3 tools/validation/validate_client_server_app.py --client-bundle-root build/debug-linux/client/bundle --server-bundle-root build/debug-linux/server/bundle
python3 tools/validation/validate_client_server_app_readiness.py --client-bundle-root build/debug-linux/client/bundle --world-blocks-path build/debug-linux/server/validation/client-server-app-launch-probe-world/world_blocks.json --chunk-view-intent-path build/debug-linux/server/validation/client-server-app-launch-probe-world/chunk_view_intent.json --chunk-stream-path build/debug-linux/server/validation/client-server-app-launch-probe-world/chunk_stream.json --player-input-intent-path build/debug-linux/server/validation/client-server-app-launch-probe-world/player_input_intent.json --block-interaction-intent-path build/debug-linux/server/validation/client-server-app-launch-probe-world/block_interaction_intent.json --log-file logs/server/octaryn_client_server_app_launch_probe-debug-linux.log
python3 tools/validation/validate_client_app_launch_probe_log.py --log-file logs/client/octaryn_client_app_launch_probe-debug-linux.log
python3 tools/validation/validate_client_shader_bundle.py --source-root octaryn-client/Shaders --bundle-shader-root build/debug-linux/client/bundle/Client/Shaders
python3 tools/validation/validate_package_policy_sync.py
python3 tools/validation/validate_cmake_policy_separation.py --repo-root .
python3 tools/validation/validate_cmake_dependency_aliases.py --repo-root .
python3 tools/validation/validate_native_abi_contracts.py
python3 tools/validation/validate_native_owner_boundaries.py --repo-root .
python3 tools/validation/validate_cmake_target_inventory.py --build-dir build/debug-linux/cmake
python3 tools/validation/validate_hostfxr_bridge_exports.py --owner client --bundle-dir build/debug-linux/client/bundle --bridge build/debug-linux/client/native/lib/liboctaryn_client_managed_bridge.so
python3 tools/validation/validate_hostfxr_bridge_exports.py --owner server --bundle-dir build/debug-linux/server/bundle --bridge build/debug-linux/server/native/lib/liboctaryn_server_managed_bridge.so
python3 tools/validation/validate_owner_launch_probe_logs.py --owner client --log-file logs/client/octaryn_client_launch_probe-debug-linux.log
python3 tools/validation/validate_owner_launch_probe_logs.py --owner server --log-file logs/server/octaryn_server_launch_probe-debug-linux.log
python3 tools/validation/validate_server_live_debug_probe_log.py --log-file logs/server/octaryn_server_live_debug_probe-debug-linux.log
```

## Notes

- Root CMake currently builds managed owner targets, the shared native ABI library, client/server native bridge facades, native owner aggregate targets, and basegame/client/server publish bundles. `octaryn_all` is build-only; `octaryn_validate_all` runs direct module policy validators, bundle payload validation, client server app readiness validation, client app launch validation, client shader bundle validation, basegame block catalog and worldgen content validation, .NET owner validation, CMake target inventory validation, native jobs/scheduler policy validation, server world generation validation, hostfxr bridge validation, and owner launch probe validation.
- Debug builds stage first-class root tools through `octaryn_debug_tools`: the PySide workspace control app, Tracy wrapper, shared tool environment, native UI launchers, setup helpers, Podman builder files, and Podman build wrappers under `build/<preset>/tools/`. RenderDoc is intentionally not workspace-managed; use an external RenderDoc install when needed.
- `Octaryn.DotNet.sln` includes shared/client/server/basegame plus the validation probe projects, so solution restore/build covers the managed validators used by CMake.
- Owner launch probe validation runs native client/server probe executables, calls valid bridge initialize/tick/command/snapshot/shutdown paths, writes logs under `logs/client/` and `logs/server/`, and validates durable server live-debug coverage for chunk generation, player spawn alignment, command queueing, block place/break edits, persistence dirtiness, ticks, and snapshot drains.
- The graphical client launcher is a client-owned native executable copied into `build/<preset>/client/bundle/` as `Octaryn.Client`. `octaryn_validate_client_app_launch_probe` runs it with SDL's dummy video driver, a bounded frame count, a deterministic validation input probe, the bundled-server chunk stream written by the `client_server_app` readiness probe, and validation-only render checks, then validates the log for window startup, bundled module descriptor/catalog/atlas loading, managed client host initialization, active live input/camera/movement/interaction/presentation diagnostics, 32-distance chunk-view/player/block intent file output, server-process chunk stream ingestion, native-drained atlas-tile presentation drawing, mipmapped material atlas uploads, direct-indirect retained mesh upload/draw, render-distance far plane, non-clear SDL GPU sky-pixel readback, block-frame presentation, mesh upload counters, ticks, and clean shutdown without linking client native code to server ABI. The target also feeds the client-written chunk-view/player/block intent files back into the bundled server process and validates the returned server-owned stream, persisted edit, chunk-boundary markers, and break/place authority logs.
- Old architecture CMake is not part of the active new-architecture validation matrix. Run it only when a task explicitly touches the transitional native host.
- Native jobs validation includes `octaryn_native_jobs_probe`, a C++ probe that validates worker-policy limits, scheduler resource conflicts, native command-write scope gating, main-thread blocking policy, native scheduled-runtime dependency/conflict wave execution, and Taskflow dependency/barrier execution through the loadable native jobs target. Native host-executed probes run on Linux/x64 and are skipped in cross-target validate graphs.
- Runtime/profiling acceptance now includes client app launch-probe evidence that server-stream mesh build runs through `octaryn_native_schedule_runtime_execute` as a worker job and SDL GPU mesh upload runs as a main-thread scheduled job. Broader frame/tick work routing, barrier behavior, and worker scaling under sustained 32-distance gameplay load remain open.
- Client process-file chunk streaming uses the same render-distance step ladder as runtime controls after the initial server preflight stream, so the launch probe now verifies the first client-authored expansion request reaches radius 4 while targeting radius 32.
- The live bundled-server stream bridge delegates duplicate metadata-only write tracking to native server chunk-stream owner code; sustained 32-distance probes should show unchanged-window skips in the server log instead of recurring large chunk-stream JSON rewrites.
- Client chunk mesh plan validation includes `octaryn_client_chunk_mesh_plan_probe`, a C++ probe that validates native terrain mesh preserve/load/unload planning, center-priority ordering, urgent/regular budgets, 32-render-distance planning scale, native-jobs worker/scheduler policy routing for mesh-build versus GPU-upload handoff, native scheduled-runtime routing for parse/build/pack/upload phases, and Taskflow execution of the planned work items. Native host-executed probes run on Linux/x64 and are skipped in cross-target validate graphs.
- Client empty-world mesh validation includes `octaryn_client_empty_world_mesh_probe`, a C++ probe that validates server-seed terrain emits lower and surface-depth chunks and edited adjacent solid overrides cull their shared internal faces.
- Server host-policy native validation includes `octaryn_server_host_policy_probe`, a C++ probe that validates server/client module-disable environment policy, live process-stream mode detection, metadata-only process-stream flag parsing, live stream path lookup, native startup host-frame construction, and native live process-stream callback loop stop behavior. Managed `Host` and process-stream code are orchestration/log glue over the loadable `octaryn_server_host` owner library, which is copied into the server bundle and resolved with `OCTARYN_SERVER_HOST_LIBRARY` when injected by validation.
- Server world-time native validation includes `octaryn_server_world_time_probe`, a C++ probe that validates the server-owned native clock/default snapshot/date carry/frame tick advancement/calendar/blob semantics mapped from old `core/world_time`, plus world-time intent process reason labels.
- Server authority-tick native validation includes `octaryn_server_authority_tick_probe`, a C++ probe that validates the server-owned native schedule plan for client command drain before player simulation before world-time advancement, worker dependency waves, schedule-report validation, and callback failure short-circuiting. Managed `AuthorityTickRunner` is callback/interop glue over the loadable `octaryn_server_authority_tick` owner library, which is copied into the server bundle and injected into managed validation probes with `OCTARYN_SERVER_AUTHORITY_TICK_LIBRARY`.
- Server block-store native validation includes `octaryn_server_block_store_probe`, a C++ probe that validates centered world bounds, floor chunk/local mapping, override edits, preserved air overrides, deterministic snapshots, chunk-column snapshots, load semantics, generated-override cleanup, native block-edit apply/support/cascade semantics, native chunk stream count/fill semantics, process stop-reason labels, block-change queue drain semantics, client block-command queue semantics and edit labels, client block-interaction hit/reach/adjacency semantics, and replication-change packing. The managed server block store/block edit service/change queue/client command queue/chunk stream provider are narrow native-handle glue over the loadable `octaryn_server_block_store` owner library, which is copied into the server bundle and injected into managed validation probes with `OCTARYN_SERVER_BLOCK_STORE_LIBRARY`.
- Server player-simulation native validation includes `octaryn_server_player_simulation_probe`, a C++ probe that validates native default spawn state, control-mode live-log labels, saved-state loading and save-change thresholds, spawn surface alignment, old-architecture walk/fly movement constants, jump/gravity behavior, AABB wall collision, input-intent process reason labels, and idle/no-input velocity clearing. The managed server player controller now uses this native owner library for default state, movement/collision, control-mode labels, save-change decisions, and idle updates while keeping persistence and live-debug logging in server glue.
- Server terrain-generation native validation includes `octaryn_server_terrain_generation_probe`, a C++ probe that validates native terrain column planning, generated-block selection, floor chunk-local mapping, material block rules, water fill, and native empty-world sampling. The managed server terrain generator is narrow material-rule interop glue over the loadable `octaryn_server_terrain_generation` owner library, which is copied into the server bundle and injected into managed validation probes with `OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY`.
- Server world-persistence native validation includes `octaryn_server_world_persistence_probe`, a C++ probe that validates native edited-block chunk-column origin planning, per-column block ordering, chunk override directory scanning, stale sidecar pruning, sidecar directory writing, world-block aggregate/sidecar load-source and save coordination, save-export bundle import writes through native path policy, save-export chunk loading/planning, metadata discovery through native path policy, aggregate chunk-column counting, gzip round trips, player save JSON read/write, player directory scan/export ordering, player directory entry writes/path shaping, native persistence root/path policy, root-relative world-save child paths, aggregate sidecar directory derivation, world-time JSON read/write, and world metadata JSON read/write behavior. The same loadable `octaryn_server_world_persistence` owner library is copied into the server bundle and injected into managed validation probes with `OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY`.
- Module sandbox validation has both source-level Roslyn checks and post-build binary metadata checks. The binary probe rejects generated or binary-only module references to denied framework surfaces such as filesystem, networking, process, reflection/dynamic loading, native interop, threading, console, and environment APIs.
- Basegame has checked-in package descriptor metadata at `octaryn-basegame/Data/Module/octaryn.basegame.module.json`. `Octaryn.ModuleManifestProbe` compares that descriptor with `ModuleRegistration.Manifest` and writes the generated validation manifest under `build/<preset>/basegame/generated/octaryn.basegame.manifest.json`.
- Bundle payload validation reads the bundled descriptor from client/server bundles and verifies every manifest-declared content and asset file is present under the same bundle root.
- Native C/C++ bridge loader validation is active for current facades. The matrix builds client/server managed outputs and native bridge facades, then verifies hostfxr startup, runtimeconfig/deps discovery, exact managed method resolution, exported owner ABI names, and invalid-input managed return paths for client and server bridge entry points.
- Toolchain configure coverage is exactly Linux and Windows, each with debug/release presets. `OCTARYN_TARGET_ARCH` is an internal dimension with `x64` as the public-preset default and `arm64` using `build/<preset>-arm64/` owner outputs. `tools/build/podman_build.sh build-all` builds every active preset for both `x64` and `arm64`. Linux native builds are Clang-only in active lanes; GCC is not an active preset lane. Windows builds are cross-builds produced inside the Linux/Arch Podman builder with LLVM MinGW. Windows host build tooling is not supported. All active UI-driven builds run inside the Arch Podman builder on Linux. Cross presets disable hostfxr bridge/probe targets when target-compatible .NET native hosting assets are not present under `OCTARYN_DOTNET_ROOT`; Linux host validation still builds and runs those bridge/probe targets.
- .NET native hosting discovery is target-RID constrained. Arch Linux x64 uses `linux-x64`, Linux arm64 uses `linux-arm64`, and Windows Clang declares `win-x64` or `win-arm64`; host pack lookup only searches `Microsoft.NETCore.App.Host.<rid>/.../runtimes/<rid>/native`, and hostfxr lookup uses the target platform extension.
- `octaryn_validate_cmake_targets` validates every configured preset graph listed in `CMakePresets.json` and rejects stale generated root buckets outside the documented owner layout.
- Native archive validation proves target ABI explicitly: x64 Linux requires `elf64-x86-64`, Linux arm64 requires `elf64-littleaarch64`, Windows x64 requires `pe-x86-64`, and Windows arm64 requires `COFF-ARM64`/`IMAGE_FILE_MACHINE_ARM64` through `llvm-readobj`.
- Linux ARM64 uses the workspace-managed sysroot under `build/dependencies/sysroots/linux-arm64/root`. The configure wrapper creates it automatically for Linux ARM64 when `OCTARYN_LINUX_ARM64_SYSROOT` is not already set.
- Direct MSBuild output is preset-partitioned by `OctarynBuildPresetName`, defaulting to `debug-linux` outside CMake. CMake sets it to the active preset so package assets, intermediate files, and managed binaries do not leak across debug/release/toolchain graphs.
