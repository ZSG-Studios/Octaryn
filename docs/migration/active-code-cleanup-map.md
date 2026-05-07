# Active Code Cleanup Map

This map records the first cleanup round for the active Octaryn owners. It is a source-to-destination plan before mechanical splits, not a behavior redesign.

## First Round Inventory

Active source/code files over 500 physical lines at the start of this map:

- `octaryn-client/Source/Native/App/octaryn_client_app.cpp` - 5422 lines; mixes client app orchestration, input, singleplayer server supervision, JSON file contracts, shader loading, world snapshot streaming, block interaction, native-empty mesh construction, GPU upload, render passes, UI overlay dispatch, live diagnostics, and frame loop execution.
- `tools/validation/Octaryn.ModuleApiProbe/Program.cs` - 1302 lines; combines API allowlist fixtures, probe execution, and reporting.
- `tools/validation/Octaryn.SchedulerProbe/Program.cs` - was 1124 lines before the managed scheduler probe was removed.
- `tools/ui/workspace_control_app.py` - was 1004 lines; split into entrypoint, layout, paths, status, logs, commands, process control, and window shell modules.
- `octaryn-shared/Source/GameModules/GameModuleValidator.cs` - was 907 lines; split into public flow, declaration validation, schedule validation, and shared rule helpers.
- `tools/validation/Octaryn.ServerWorldBlocksProbe/Program.cs` - 805 lines.
- `cmake/Owners/ClientTargets.cmake` - 779 lines; combines client native libraries, managed owner targets, shader staging, bundling, app launch probes, and validation targets.
- `octaryn-client/Shaders/ui.comp.glsl` - was 731 lines; split into focused UI shader includes.
- `octaryn-basegame/Tools/build_atlas_from_pack.py` - was 711 lines; split into focused basegame atlas-builder modules.
- `octaryn-client/Source/Host/HostScheduler.cs` - was 665 lines before the managed scheduler was removed.
- `tools/validation/validate_cmake_target_inventory.py` - 661 lines.
- `octaryn-server/Source/Tick/HostScheduler.cs` - was 651 lines before the managed scheduler was removed.
- `cmake/Owners/ToolTargets.cmake` - 608 lines.
- `tools/validation/validate_client_server_app_readiness.py` - 563 lines.
- `tools/Source/ShaderCompiler/ShaderCompilerMain.cpp` - 551 lines.
- `octaryn-client/Source/Rendering/BlockAtlas/BlockAtlas.cpp` - 549 lines.
- `octaryn-server/Source/Modules/ModuleActivator.cs` - was 546 lines before the server host/module cleanup.
- `octaryn-client/Source/Native/Ui/RuntimeControls/octaryn_client_runtime_controls.cpp` - 543 lines.
- `tools/validation/Octaryn.OwnerModuleValidationProbe/Program.cs` - 515 lines.

## AAA Source-To-Destination Plan

Analyze:

- Start with the client app because it is the largest active monolith and blocks further client runtime work.
- Keep behavior-preserving splits first; do not move ownership across client/server/shared/basegame boundaries.
- Preserve the in-flight client function profiling work and move code around it instead of reverting it.

Assign:

- `octaryn-client/Source/Native/App/octaryn_client_app.cpp`
  - `octaryn-client/Source/Native/App/JsonFiles/` for launch-probe and stream JSON records.
  - `octaryn-client/Source/Native/App/FileIO/` for app-local text/binary/atomic file helpers.
  - `octaryn-client/Source/Native/App/SingleplayerServerSession/` for bundled server path setup, environment handoff, spawn, and shutdown.
  - `octaryn-client/Source/Native/App/Input/` for SDL key/pointer input, validation probe input, host frame input filling, and input diagnostics.
  - `octaryn-client/Source/Native/App/WorldStream/` for server chunk stream reads, world block records, local block overrides, and intent writes.
  - `octaryn-client/Source/Rendering/WorldMeshUpload/` for mesh upload frames, GPU buffers, chunk mesh merges, and upload calls.
  - `octaryn-client/Source/Rendering/EmptyWorldMesh/` for native-empty world mesh construction.
  - `octaryn-client/Source/Native/App/FrameTargets/`, `ShaderWorldPass/`, `CompositePass/`, and `FrameRender/` for current app-owned render pass orchestration until render ownership is split further.
  - `octaryn-client/Source/Native/App/UiOverlayPass/` and `UiOverlayUniforms/` for UI overlay compute dispatch and metrics packing.
- `cmake/Owners/ClientTargets.cmake`
  - `cmake/Owners/ClientTargets/ClientBuildPaths.cmake` for client build roots, bundle paths, and probe paths.
  - `cmake/Owners/ClientTargets/ClientNativeLibraryTargets.cmake` for client native library declarations.
  - `cmake/Owners/ClientTargets/ClientShaderTargets.cmake` for client shader staging and bundle-output enumeration.
  - `cmake/Owners/ClientTargets/ClientHostAppTargets.cmake` for native managed bridge, launch probe, and graphical app targets.
  - `cmake/Owners/ClientTargets/ClientManagedBundleTargets.cmake` for managed client publish, game module payloads, bundled server copy, and `octaryn_client_bundle`.
  - `cmake/Owners/ClientTargets/ClientLaunchProbeTargets.cmake` for client launch-probe run/validation targets.
- `cmake/Owners/ToolTargets.cmake`
  - `cmake/Owners/ToolTargets/ToolBuildPaths.cmake` for tool-owned build roots, logs, probe paths, and asset paths.
  - `cmake/Owners/ToolTargets/ToolNativeTargets.cmake` for repo-wide native tool targets.
  - `cmake/Owners/ToolTargets/ToolDebugStagingTargets.cmake` for debug tool staging.
  - `cmake/Owners/ToolTargets/ToolCmakeValidationTargets.cmake` for CMake/policy validation targets.
  - `cmake/Owners/ToolTargets/ToolModuleValidationTargets.cmake` for module, package, basegame, and managed API validation targets.
  - `cmake/Owners/ToolTargets/ToolBundleValidationTargets.cmake` for client/server bundle and native boundary validation targets.
  - `cmake/Owners/ToolTargets/ToolOwnerProbeValidationTargets.cmake` for owner launch probes, hostfxr checks, and owner probe targets.
  - `cmake/Owners/ToolTargets/ToolAggregateTargets.cmake` for `octaryn_tools` aggregate dependencies.
- Validation/tool monoliths remain queued after the client app split because they are less likely to block runtime feature work.

## Client App Bucket Move Round

The first app split is complete, and the app bucket was moved out of top-level `Source/Native` without behavior changes. The `octaryn_client_app` CMake target remains stable, while the source now lives under focused `octaryn-client/Source/App/` folders.

Source-to-destination map:

- `octaryn-client/Source/Native/App/octaryn_client_app.cpp` -> `octaryn-client/Source/App/HostApp.cpp`.
- `Source/Native/App/Environment/Environment.*` -> `Source/App/Environment/Environment.*`.
- `Source/Native/App/FileIO/FileIO.*` -> `Source/App/RuntimeFiles/FileIO.*`.
- `Source/Native/App/JsonFiles/JsonFiles.h` -> `Source/App/RuntimeFiles/JsonContracts.h`.
- `Source/Native/App/SingleplayerServerSession/octaryn_singleplayer_server_session.*` -> `Source/App/SingleplayerServerSession/SingleplayerServerSession.*`.
- `Source/Native/App/Input/Input.*` -> `Source/App/Input/Input.*`.
- `Source/Native/App/EventPump/EventPump.*` -> `Source/App/EventPump/EventPump.*`.
- `Source/Native/App/FrameLoop/FrameLoop.*` -> `Source/App/FrameLoop/FrameLoop.*`.
- `Source/Native/App/FrameLogs/FrameLogs.*` -> `Source/App/FrameLogs/FrameLogs.*`.
- `Source/Native/App/HostCommands/HostCommands.*` -> `Source/App/HostCommands/HostCommands.*`.
- `Source/Native/App/WorldIntents/WorldIntents.*` -> `Source/App/WorldIntents/WorldIntents.*`.
- `Source/Native/App/WorldStream/WorldStream.*` -> `Source/App/WorldStream/WorldStream.*`.
- `Source/Native/App/PresentationState/PresentationState.*` -> `Source/App/PresentationState/PresentationState.*`.
- `Source/Native/App/PresentationSnapshots/PresentationSnapshots.*` -> `Source/App/PresentationSnapshots/PresentationSnapshots.*`.
- `Source/Native/App/BlockInteraction/BlockInteraction.*` -> `Source/App/BlockInteraction/BlockInteraction.*`.
- `Source/Native/App/AtlasFallbackDraw/AtlasFallbackDraw.*` -> `Source/App/Rendering/AtlasFallbackDraw/AtlasFallbackDraw.*`.
- `Source/Native/App/CompositePass/CompositePass.*` -> `Source/App/Rendering/CompositePass/CompositePass.*`.
- `Source/Native/App/EmptyWorldAtlas/EmptyWorldAtlas.*` -> `Source/App/Rendering/EmptyWorldAtlas/EmptyWorldAtlas.*`.
- `Source/Native/App/FrameRender/FrameRender.*` -> `Source/App/Rendering/FrameRender/FrameRender.*`.
- `Source/Native/App/FrameTargets/FrameTargets.*` -> `Source/App/Rendering/FrameTargets/FrameTargets.*`.
- `Source/Native/App/ShaderPipelines/ShaderPipelines.*` -> `Source/App/Rendering/ShaderPipelines/ShaderPipelines.*`.
- `Source/Native/App/ShaderWorldPass/ShaderWorldPass.*` -> `Source/App/Rendering/ShaderWorldPass/ShaderWorldPass.*`.
- `Source/Native/App/SkyUniforms/SkyUniforms.*` -> `Source/App/Rendering/SkyUniforms/SkyUniforms.*`.
- `Source/Native/App/UiOverlayPass/UiOverlayPass.*` -> `Source/App/UiOverlay/UiOverlayPass.*`.
- `Source/Native/App/UiOverlayUniforms/UiOverlayUniforms.*` -> `Source/App/UiOverlay/UiOverlayUniforms.*`.
- `Source/Native/App/Window/Window.*` -> `Source/App/Window/Window.*`.

Move requirements:

- Update `cmake/Owners/ClientTargets/ClientHostAppTargets.cmake` source and include lists in the same mechanical commit.
- Update includes for renamed `JsonContracts.h` and `SingleplayerServerSession.h`.
- Keep exported ABI symbols and the `octaryn_client_app` CMake target stable.
- Delete `octaryn-client/Source/Native/App/` after the move if it is empty.
- Validate with `tools/build/cmake_configure.sh debug-linux`, `tools/build/cmake_build.sh debug-linux --target octaryn_client_app`, direct owner-boundary and target-inventory validators, old-folder removal checks, `git diff --check`, and active line-count checks.

## Client Native Support Leaf Rounds

Client-owned support libraries are moving out of top-level `Source/Native` as focused leaf rounds. CMake target names stay stable for inventory and downstream validation, but file names and internal C/C++ APIs should be path-aware once the files live under behavior folders.

Completed source-to-destination maps:

- `octaryn-client/Source/Native/DisplayCatalog/DisplayCatalog.*` -> `octaryn-client/Source/Display/DisplayCatalog/DisplayCatalog.*`.
- `octaryn-client/Source/Native/FrameMetrics/octaryn_client_frame_metrics.*` -> `octaryn-client/Source/Diagnostics/FrameMetrics/FrameMetrics.*`.
- `octaryn-client/Source/Native/WorldStreaming/octaryn_client_chunk_view.*` -> `octaryn-client/Source/WorldPresentation/ChunkView/ChunkView.*`.
- `octaryn-client/Source/Native/ClientHost/Environment/octaryn_client_host_environment.*` -> `octaryn-client/Source/Host/Environment/HostEnvironment.*`.
- `octaryn-client/Source/Native/Diagnostics/FrameProfile/octaryn_client_frame_profile.*` -> `octaryn-client/Source/Diagnostics/FrameProfile/FrameProfile.*`.
- `octaryn-client/Source/Native/Diagnostics/FunctionProfile/octaryn_client_function_profile.*` -> `octaryn-client/Source/Diagnostics/FunctionProfile/FunctionProfile.*`.
- `octaryn-client/Source/Native/Window/FramePacing/octaryn_client_frame_pacing.*` -> `octaryn-client/Source/Window/FramePacing/FramePacing.*`.
- `octaryn-client/Source/Native/Window/FrameStatistics/octaryn_client_window_frame_statistics.*` -> `octaryn-client/Source/Window/FrameStatistics/FrameStatistics.*`.
- `octaryn-client/Source/Native/Window/FullscreenDisplayMode/octaryn_client_fullscreen_display_mode.*` -> `octaryn-client/Source/Window/FullscreenDisplayMode/FullscreenDisplayMode.*`.
- `octaryn-client/Source/Native/Window/Lifecycle/octaryn_client_window_lifecycle.*` -> `octaryn-client/Source/Window/Lifecycle/Lifecycle.*`.
- `octaryn-client/Source/Native/Window/Swapchain/octaryn_client_swapchain.*` -> `octaryn-client/Source/Window/Swapchain/Swapchain.*`.
- `octaryn-client/Source/Native/Input/PlayerControl/octaryn_client_player_control_input.*` -> `octaryn-client/Source/Input/PlayerControl/PlayerControlInput.*`.
- `octaryn-client/Source/Native/Player/FlyController/octaryn_client_fly_player_controller.*` -> `octaryn-client/Source/Player/FlyController/FlyPlayerController.*`.
- `octaryn-client/Source/Native/Settings/AppSettings/octaryn_client_app_settings.*` -> `octaryn-client/Source/Settings/AppSettings/AppSettings.*`.
- `octaryn-client/Source/Native/Settings/DisplaySettings/octaryn_client_display_settings.*` -> `octaryn-client/Source/Settings/DisplaySettings/DisplaySettings.*`.
- `octaryn-client/Source/Native/Settings/LightingSettings/octaryn_client_lighting_settings.*` -> `octaryn-client/Source/Settings/LightingSettings/LightingSettings.*`.
- `octaryn-client/Source/Native/Settings/RenderDistance/octaryn_client_render_distance.*` -> `octaryn-client/Source/Settings/RenderDistance/RenderDistance.*`.
- `octaryn-client/Source/Native/Settings/RuntimeSettings/octaryn_client_runtime_settings.*` -> `octaryn-client/Source/Settings/RuntimeSettings/RuntimeSettings.*`.
- `octaryn-client/Source/Native/Ui/DisplayMenu/*` -> `octaryn-client/Source/Ui/DisplayMenu/*`.
- `octaryn-client/Source/Native/Ui/RuntimeControls/*` -> `octaryn-client/Source/Ui/RuntimeControls/*`.
- `octaryn-client/Source/Native/Rendering/BlockAtlas/*` -> `octaryn-client/Source/Rendering/BlockAtlas/*`.
- `octaryn-client/Source/Native/Rendering/Camera/octaryn_client_camera.*` -> `octaryn-client/Source/Rendering/Camera/Camera.*`.
- `octaryn-client/Source/Native/Rendering/Camera/octaryn_client_camera_matrix.*` -> `octaryn-client/Source/Rendering/Camera/CameraMatrix.*`.
- `octaryn-client/Source/Native/Rendering/EmptyWorldMesh/*` -> `octaryn-client/Source/Rendering/EmptyWorldMesh/*`.
- `octaryn-client/Source/Native/Rendering/Scene/HiddenBlocks/octaryn_client_hidden_block_uniforms.*` -> `octaryn-client/Source/Rendering/Scene/HiddenBlocks/HiddenBlockUniforms.*`.
- `octaryn-client/Source/Native/Rendering/Shaders/Create/octaryn_client_shader_creation.*` -> `octaryn-client/Source/Rendering/Shaders/Create/ShaderCreation.*`.
- `octaryn-client/Source/Native/Rendering/Shaders/Metadata/octaryn_client_shader_metadata_contract.*` -> `octaryn-client/Source/Rendering/Shaders/Metadata/ShaderMetadataContract.*`.
- `octaryn-client/Source/Native/Rendering/Visibility/octaryn_client_visibility_flags.*` -> `octaryn-client/Source/Rendering/Visibility/VisibilityFlags.*`.
- `octaryn-client/Source/Native/Rendering/WorldMeshUpload/octaryn_client_world_mesh_upload.*` -> `octaryn-client/Source/Rendering/WorldMeshUpload/WorldMeshUpload.*`.

No client/server top-level `Source/Native` or `Source/Managed` language buckets remain after these rounds.

## Server Persistence Naming Cleanup

Player persistence now uses path-aware names inside `octaryn-server/Source/Persistence/Players/`:

- `ServerPlayerPersistence.cs` -> `PlayerPersistence.cs`.
- `ServerPlayerSaveFile.cs` -> `PlayerSaveFile.cs`.
- `ServerPlayerSaveState.cs` -> `PlayerSaveState.cs`.

The save file shape and environment variables keep their existing names because they are runtime/persistence contracts, not source ownership labels.

World-save persistence now uses path-aware names inside `octaryn-server/Source/Persistence/WorldSave/`:

- `ServerSaveExportBundleFile.cs` -> `SaveExportBundleFile.cs`.
- `ServerWorldSaveMetadata.cs` -> `WorldSaveMetadata.cs`.
- `ServerWorldSaveMetadataBuilder.cs` -> `WorldSaveMetadataBuilder.cs`.
- `ServerWorldSaveMetadataFile.cs` -> removed after native world-persistence took over metadata file IO.

The gzip export format and metadata JSON shape keep their existing versioned contracts.

World-block persistence now uses the path-aware name `WorldBlockPersistence` inside `octaryn-server/Source/Persistence/WorldBlocks/`, replacing `ServerWorldBlockPersistence`.

## Shared Host ABI Bucket Move

The shared host ABI contract is no longer inside a top-level implementation-language bucket:

- `octaryn-shared/Source/Native/HostAbi/octaryn_host_abi.c` -> `octaryn-shared/Source/HostAbi/octaryn_host_abi.c`.
- `octaryn-shared/Source/Native/HostAbi/octaryn_host_abi.h` -> `octaryn-shared/Source/HostAbi/octaryn_host_abi.h`.
- `octaryn-shared/Source/Native/HostAbi/octaryn_shared_abi_types.h` -> `octaryn-shared/Source/HostAbi/octaryn_shared_abi_types.h`.

The exported C ABI symbol names and header names stay stable because they are cross-owner ABI contracts.

## Server Player Simulation Naming Cleanup

Player simulation now uses path-aware names inside `octaryn-server/Source/Simulation/Players/`:

- `ServerPlayerController.cs` -> `PlayerController.cs`.
- `ServerPlayerCollision.cs` -> `PlayerCollision.cs`.
- `ServerPlayerControlMode.cs` -> `PlayerControlMode.cs`.
- `ServerPlayerState.cs` -> `PlayerState.cs`.
- `ServerPlayerInputIntentFile.cs` -> `PlayerInputIntentFile.cs`.

Runtime log keys and chunk-stream JSON shapes keep their existing `server`/`client` wording because they describe live contracts rather than source ownership.

## Server World Naming Cleanup

Server world code now uses path-aware names inside `octaryn-server/Source/World/`:

- `World/Blocks/ServerBlock*.cs` -> `World/Blocks/Block*.cs`.
- `World/Blocks/ServerChunkBlocks.cs` -> `World/Blocks/ChunkBlocks.cs`.
- `World/Blocks/ServerClientBlockCommandQueue.cs` -> `World/Blocks/ClientBlockCommandQueue.cs`.
- `World/Blocks/ServerDenyBlockAuthorityRules.cs` -> `World/Blocks/DenyBlockAuthorityRules.cs`.
- `World/Blocks/ServerNativeEmptyWorldBlockAuthorityRules.cs` -> `World/Blocks/NativeEmptyWorldBlockAuthorityRules.cs`.
- `World/Chunks/ServerChunk*.cs` -> `World/Chunks/Chunk*.cs`.
- `World/Chunks/ServerWorldTimeIntentFile.cs` -> `World/Chunks/WorldTimeIntentFile.cs`.
- `World/Generation/Server*.cs` -> path-aware generation names such as `TerrainGenerator.cs`, `TerrainNoise.cs`, and `InitialWorldSeeder.cs`.

`Client*` names remain where they describe client-originated commands or view intent contracts rather than server ownership.

## Basegame Naming Cleanup

Basegame source now uses path-aware names inside `octaryn-basegame/Source/`:

- `BasegameBlockCatalog.cs` -> `BlockCatalog.cs`.
- `BasegameFluidKind.cs` -> `FluidKind.cs`.
- `BasegameWorldGenerationRules.cs` -> `WorldGenerationRules.cs`.
- `BasegameBlockAuthorityRules.cs` -> `BlockAuthorityRules.cs`.
- `BasegameFrameState.cs` -> `FrameState.cs`.
- `BasegameEntityTag.cs` -> `EntityTag.cs`.
- `BasegameModuleRegistration.cs` -> `ModuleRegistration.cs`.
- `BasegameScheduleDeclarations.cs` -> `ScheduleDeclarations.cs`.

The module ID, asset IDs, content IDs, save compatibility ID, generated manifest, and runtime package descriptor keep `octaryn.basegame` because those are product/content identity contracts.

## Loose File Folder Sorting

Large flat owner folders were split into focused subfolders without changing namespaces or runtime behavior. The later C# engine-system cleanup removed the managed client presentation/meshing implementation; active client world presentation now keeps only the native `ChunkView/` slice until the remaining old-architecture presentation systems are ported in C++.

- `octaryn-client/Source/WorldPresentation/` currently keeps native `ChunkView/`; managed `Blocks/`, `Meshing/`, `Packing/`, `Snapshots/`, and `Uploads/` were removed because those were C# engine systems rather than module bridge/API glue.
- `octaryn-client/Source/Host/` is split into `Modules/`, `Scheduling/`, and `Snapshots/`.
- `octaryn-client/Source/HostBridge/` is split into `Abi/`, `Commands/`, `Exports/`, `LaunchProbe/`, and `NativeLoading/`.
- `octaryn-client/Source/Rendering/BlockAtlas/` is split into `Atlas/`, `Files/`, and `Textures/`.
- `octaryn-client/Source/Rendering/EmptyWorldMesh/` is split into `Geometry/`, `Packing/`, and `View/`.
- `octaryn-client/Source/Ui/RuntimeControls/` is split into `Entrypoints/`, `Events/`, and `Menu/`.
- `octaryn-server/Source/HostBridge/` is split into `Abi/`, `Exports/`, `LaunchProbe/`, and `NativeLoading/`.
- `octaryn-server/Source/Persistence/WorldBlocks/` is split into `ChunkColumns/` and `WorldOverrides/`.
- `octaryn-server/Source/Tick/` scheduler files are now under `Tick/Scheduling/`.
- `octaryn-server/Source/World/Blocks/` is split into `Authority/`, `Commands/`, and `Store/`, with command files further grouped by editing, queueing, and interaction intent.
- `octaryn-server/Source/World/Chunks/` currently keeps the active stream bridge under `Streaming/`; stale `IntentFiles/` and `Windowing/` buckets have been removed.
- `octaryn-shared/Source/GameModules/` is split into `Commands/`, `Manifest/`, `Providers/`, `Runtime/`, `Scheduling/`, and `Validation/`, with manifest and validation subgroups.
- `octaryn-shared/Source/Host/` is split into `Commands/`, `Frames/`, `Scheduling/`, and `Threading/`, with scheduler/work/resource subgroups.
- `octaryn-shared/Source/Networking/` is split into `Commands/`, `Messages/`, `Replication/`, `Requests/`, and `Snapshots/`.
- `octaryn-shared/Source/World/` is split into `Blocks/`, `Chunks/`, `Positions/`, and `Terrain/`.

## Validation Tool Cleanup Round

`tools/validation/Octaryn.ModuleApiProbe/Program.cs` was the largest remaining validation monolith. Its behavior stays as the same `octaryn_validate_module_source_api` executable target, but responsibilities are split as follows:

- `Program.cs` keeps only CLI entrypoint, argument parsing, and error reporting.
- `ModuleApiProbePolicy.cs` owns framework API allow/deny policy tables and group lookup helpers.
- `ModuleApiProbeValidation.cs` owns Roslyn source validation passes and diagnostic formatting.
- `ModuleApiProbeManifest.cs` owns manifest requested-framework extraction and metadata-reference loading.
- `ModuleApiProbeSelfTests.cs` owns the self-test case list.
- `ModuleApiProbeSelfTestFixtures.cs` owns temporary module fixture creation and self-test assertions.

`tools/validation/Octaryn.SchedulerProbe/`, `validate_scheduler_contract.py`, and the managed client/server `HostScheduler` implementations were removed. Scheduling is an owner/native engine system and must be validated through the C++ native jobs path instead of managed coordinator or worker-pool code.

`tools/validation/Octaryn.ServerWorldBlocksProbe/Program.cs` was split next. Its behavior stays as the same compiled server world-block probe:

- `Program.cs` keeps probe orchestration.
- `ServerWorldBlockStoreValidation.cs` owns world constants, edit/query, support rules, player collision, chunk mapping, snapshot order, and override persistence checks.
- `ServerWorldBlockCommandValidation.cs` owns host command sink, client command queue, module command path, and submitted client-command checks.
- `ServerWorldBlockSnapshotValidation.cs` owns server snapshot drain and activator persistence lifecycle checks.
- `ServerWorldBlocksProbeFixtures.cs` owns frame, persistence-path, assertion, module-registration, module-instance, and rejecting-sink fixtures.

`tools/validation/Octaryn.ModuleBinarySandboxProbe/Program.cs` was split next. Its behavior stays as the same `octaryn_validate_module_binary_sandbox` executable target:

- `Program.cs` keeps only the top-level entrypoint.
- `BinarySandboxProbe.cs` owns CLI argument parsing, self-test invocation, validation dispatch, and error reporting.
- `SelfTests.cs` owns denied/allowed API classifier checks and package-policy fixture checks.
- `AssemblyValidator.cs` owns PE metadata traversal for assembly, type, member, and P/Invoke validation.
- `AssemblyReferencePolicy.cs` owns project.assets.json package policy parsing and allowed assembly closure construction.
- `ApiClassifier.cs` owns denied framework/module API namespace and type classification.
- `FrameworkAssemblies.cs` owns trusted framework assembly reference checks.

`tools/validation/Octaryn.ModuleManifestProbe/Program.cs` was split next. Its behavior stays as the same `octaryn_validate_module_manifest_probe` executable target:

- `Program.cs` keeps only the top-level entrypoint.
- `ManifestProbe.cs` owns CLI argument parsing, self-test invocation, manifest validation dispatch, optional dump writing, and error reporting.
- `ManifestDumpWriter.cs` owns generated manifest JSON writing.
- `PackageDescriptorValidator.cs` owns package descriptor loading and generated manifest comparison.
- `SelfTests.cs` owns validator and file-graph self-test cases.
- `Fixtures.cs` owns synthetic manifest, scheduled-system, and file-writing fixtures for self-tests.
- `ManifestValidator.cs` owns manifest issue collection, declared file validation, content identity checks, and undeclared file checks.

`tools/validation/Octaryn.ClientWorldPresentationProbe/` and `octaryn_validate_client_world_presentation_probe` were removed with the managed client presentation/meshing system. Future validation for this area must target the C++ owner path and native job/upload pipeline.
- `NeighborhoodValidation.cs` owns neighborhood snapshot capture and boundary face-visibility checks.
- `RenderRulesValidation.cs` owns block render-kind and face-visibility rule checks.
- `MeshPlanningValidation.cs` owns chunk mesh planning and non-fluid planner-to-packer pipeline checks.
- `MeshPackingValidation.cs` owns packed mesh, upload descriptor, and upload record checks.
- `SnapshotConsumerValidation.cs` owns server snapshot tick-order checks.

Act:

- First extract JSON/file/session helpers from `octaryn_client_app.cpp`; these have narrow dependencies and can be validated with a client app build.
- Then extract input/intent/world stream helpers.
- Then split render/mesh/UI responsibilities once the data contracts are no longer private to the main app translation unit.
