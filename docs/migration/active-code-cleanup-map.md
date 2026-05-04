# Active Code Cleanup Map

This map records the first cleanup round for the active Octaryn owners. It is a source-to-destination plan before mechanical splits, not a behavior redesign.

## First Round Inventory

Active source/code files over 500 physical lines:

- `octaryn-client/Source/Native/App/octaryn_client_app.cpp` - 5422 lines; mixes client app orchestration, input, singleplayer server supervision, JSON file contracts, shader loading, world snapshot streaming, block interaction, native-empty mesh construction, GPU upload, render passes, UI overlay dispatch, live diagnostics, and frame loop execution.
- `tools/validation/Octaryn.ModuleApiProbe/Program.cs` - 1302 lines; combines API allowlist fixtures, probe execution, and reporting.
- `tools/validation/Octaryn.SchedulerProbe/Program.cs` - 1124 lines; combines scheduler fixtures, assertions, and probe entrypoint.
- `tools/ui/workspace_control_app.py` - 1004 lines; combines UI layout, action orchestration, build process control, and state presentation.
- `octaryn-shared/Source/GameModules/GameModuleValidator.cs` - 907 lines; combines manifest validation phases and issue creation.
- `tools/validation/Octaryn.ServerWorldBlocksProbe/Program.cs` - 805 lines.
- `cmake/Owners/ClientTargets.cmake` - 779 lines; combines client native libraries, managed owner targets, shader staging, bundling, app launch probes, and validation targets.
- `tools/validation/Octaryn.ModuleBinarySandboxProbe/Program.cs` - 776 lines.
- `octaryn-client/Shaders/ui.comp.glsl` - 731 lines.
- `octaryn-basegame/Tools/build_atlas_from_pack.py` - 711 lines.
- `tools/validation/Octaryn.ModuleManifestProbe/Program.cs` - 679 lines.
- `octaryn-client/Source/ClientHost/ClientHostScheduler.cs` - 665 lines.
- `tools/validation/validate_cmake_target_inventory.py` - 661 lines.
- `octaryn-server/Source/Tick/ServerHostScheduler.cs` - 651 lines.
- `cmake/Owners/ToolTargets.cmake` - 608 lines.
- `tools/validation/Octaryn.ClientWorldPresentationProbe/Program.cs` - 585 lines.
- `tools/validation/validate_client_server_app_readiness.py` - 563 lines.
- `tools/Source/ShaderCompiler/ShaderCompilerMain.cpp` - 551 lines.
- `octaryn-client/Source/Native/Rendering/Atlas/octaryn_client_block_atlas.cpp` - 549 lines.
- `octaryn-server/Source/Managed/ServerModuleActivator.cs` - 546 lines.
- `octaryn-client/Source/Native/Ui/RuntimeControls/octaryn_client_runtime_controls.cpp` - 543 lines.
- `tools/validation/Octaryn.OwnerModuleValidationProbe/Program.cs` - 515 lines.

## AAA Source-To-Destination Plan

Analyze:

- Start with the client app because it is the largest active monolith and blocks further client runtime work.
- Keep behavior-preserving splits first; do not move ownership across client/server/shared/basegame boundaries.
- Preserve the in-flight client function profiling work and move code around it instead of reverting it.

Assign:

- `octaryn-client/Source/Native/App/octaryn_client_app.cpp`
  - `octaryn-client/Source/Native/App/ClientAppJsonFiles/` for launch-probe and stream JSON records.
  - `octaryn-client/Source/Native/App/ClientAppFileIO/` for app-local text/binary/atomic file helpers.
  - `octaryn-client/Source/Native/App/SingleplayerServerSession/` for bundled server path setup, environment handoff, spawn, and shutdown.
  - `octaryn-client/Source/Native/App/ClientAppInput/` for SDL key/pointer input, validation probe input, host frame input filling, and input diagnostics.
  - `octaryn-client/Source/Native/App/ClientAppWorldStream/` for server chunk stream reads, world block records, local block overrides, and intent writes.
  - `octaryn-client/Source/Native/Rendering/WorldMeshUpload/` for mesh upload frames, GPU buffers, chunk mesh merges, and upload calls.
  - `octaryn-client/Source/Native/Rendering/NativeEmptyWorldMesh/` for native-empty world mesh construction.
  - `octaryn-client/Source/Native/Rendering/ClientAppRenderPasses/` for current app-owned pass orchestration until render ownership is split further.
  - `octaryn-client/Source/Native/Ui/ClientAppDebugOverlay/` for UI overlay compute dispatch and metrics packing.
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

## Validation Tool Cleanup Round

`tools/validation/Octaryn.ModuleApiProbe/Program.cs` was the largest remaining validation monolith. Its behavior stays as the same `octaryn_validate_module_source_api` executable target, but responsibilities are split as follows:

- `Program.cs` keeps only CLI entrypoint, argument parsing, and error reporting.
- `ModuleApiProbePolicy.cs` owns framework API allow/deny policy tables and group lookup helpers.
- `ModuleApiProbeValidation.cs` owns Roslyn source validation passes and diagnostic formatting.
- `ModuleApiProbeManifest.cs` owns manifest requested-framework extraction and metadata-reference loading.
- `ModuleApiProbeSelfTests.cs` owns the self-test case list.
- `ModuleApiProbeSelfTestFixtures.cs` owns temporary module fixture creation and self-test assertions.

`tools/validation/Octaryn.SchedulerProbe/Program.cs` was split next. Its behavior stays as the same compiled scheduler probe, and the scheduler contract validator now checks all probe `.cs` files instead of only `Program.cs`:

- `Program.cs` keeps owner scheduler construction and probe orchestration.
- `SchedulerProbeLifecycleValidation.cs` owns worker-count, topology, shutdown, disposal, and frame fixture checks.
- `SchedulerProbeExecutionValidation.cs` owns blocking, fire-and-forget, failure-diagnostic, nested-run, undeclared-work, and capacity checks.
- `SchedulerProbeOrderingValidation.cs` owns `RunsAfter`, `RunsBefore`, failed-prerequisite, and commit-barrier checks.
- `SchedulerProbeResourceValidation.cs` owns exact-conflict, independent-resource, and deterministic serial-resource checks.
- `SchedulerProbeDeclarations.cs` owns scheduled-system declarations and resource-access fixtures.

`tools/validation/Octaryn.ServerWorldBlocksProbe/Program.cs` was split next. Its behavior stays as the same compiled server world-block probe:

- `Program.cs` keeps probe orchestration.
- `ServerWorldBlockStoreValidation.cs` owns world constants, edit/query, support rules, player collision, chunk mapping, snapshot order, and override persistence checks.
- `ServerWorldBlockCommandValidation.cs` owns host command sink, client command queue, module command path, and submitted client-command checks.
- `ServerWorldBlockSnapshotValidation.cs` owns server snapshot drain and activator persistence lifecycle checks.
- `ServerWorldBlocksProbeFixtures.cs` owns frame, persistence-path, assertion, module-registration, module-instance, and rejecting-sink fixtures.

Act:

- First extract JSON/file/session helpers from `octaryn_client_app.cpp`; these have narrow dependencies and can be validated with a client app build.
- Then extract input/intent/world stream helpers.
- Then split render/mesh/UI responsibilities once the data contracts are no longer private to the main app translation unit.
