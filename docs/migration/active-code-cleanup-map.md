# Active Code Cleanup Map

This map records the first cleanup round for the active Octaryn owners. It is a source-to-destination plan before mechanical splits, not a behavior redesign.

## First Round Inventory

Active source/code files over 500 physical lines at the start of this map:

- `octaryn-client/Source/Native/App/octaryn_client_app.cpp` - 5422 lines; mixes client app orchestration, input, singleplayer server supervision, JSON file contracts, shader loading, world snapshot streaming, block interaction, native-empty mesh construction, GPU upload, render passes, UI overlay dispatch, live diagnostics, and frame loop execution.
- `tools/validation/Octaryn.ModuleApiProbe/Program.cs` - 1302 lines; combines API allowlist fixtures, probe execution, and reporting.
- `tools/validation/Octaryn.SchedulerProbe/Program.cs` - 1124 lines; combines scheduler fixtures, assertions, and probe entrypoint.
- `tools/ui/workspace_control_app.py` - was 1004 lines; split into entrypoint, layout, paths, status, logs, commands, process control, and window shell modules.
- `octaryn-shared/Source/GameModules/GameModuleValidator.cs` - was 907 lines; split into public flow, declaration validation, schedule validation, and shared rule helpers.
- `tools/validation/Octaryn.ServerWorldBlocksProbe/Program.cs` - 805 lines.
- `cmake/Owners/ClientTargets.cmake` - 779 lines; combines client native libraries, managed owner targets, shader staging, bundling, app launch probes, and validation targets.
- `octaryn-client/Shaders/ui.comp.glsl` - was 731 lines; split into focused UI shader includes.
- `octaryn-basegame/Tools/build_atlas_from_pack.py` - was 711 lines; split into focused basegame atlas-builder modules.
- `octaryn-client/Source/ClientHost/ClientHostScheduler.cs` - 665 lines.
- `tools/validation/validate_cmake_target_inventory.py` - 661 lines.
- `octaryn-server/Source/Tick/ServerHostScheduler.cs` - 651 lines.
- `cmake/Owners/ToolTargets.cmake` - 608 lines.
- `tools/validation/validate_client_server_app_readiness.py` - 563 lines.
- `tools/Source/ShaderCompiler/ShaderCompilerMain.cpp` - 551 lines.
- `octaryn-client/Source/Native/Rendering/BlockAtlas/BlockAtlas.cpp` - 549 lines.
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
  - `octaryn-client/Source/Native/App/JsonFiles/` for launch-probe and stream JSON records.
  - `octaryn-client/Source/Native/App/FileIO/` for app-local text/binary/atomic file helpers.
  - `octaryn-client/Source/Native/App/SingleplayerServerSession/` for bundled server path setup, environment handoff, spawn, and shutdown.
  - `octaryn-client/Source/Native/App/Input/` for SDL key/pointer input, validation probe input, host frame input filling, and input diagnostics.
  - `octaryn-client/Source/Native/App/WorldStream/` for server chunk stream reads, world block records, local block overrides, and intent writes.
  - `octaryn-client/Source/Native/Rendering/WorldMeshUpload/` for mesh upload frames, GPU buffers, chunk mesh merges, and upload calls.
  - `octaryn-client/Source/Native/Rendering/EmptyWorldMesh/` for native-empty world mesh construction.
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

`tools/validation/Octaryn.ClientWorldPresentationProbe/Program.cs` was split next. Its behavior stays as the same `octaryn_validate_client_world_presentation_probe` executable target, and stale `ClientNeighborhoodBoundaryBlocks` constructor calls were updated to the explicit boundary contract:

- `Program.cs` keeps only the top-level entrypoint.
- `PresentationProbe.cs` owns probe orchestration.
- `ProbeAssertions.cs` owns assertion helpers.
- `StoreValidation.cs` owns presentation-store update, dirty-chunk, and world-boundary checks.
- `NeighborhoodValidation.cs` owns neighborhood snapshot capture and boundary face-visibility checks.
- `RenderRulesValidation.cs` owns block render-kind and face-visibility rule checks.
- `MeshPlanningValidation.cs` owns chunk mesh planning and non-fluid planner-to-packer pipeline checks.
- `MeshPackingValidation.cs` owns packed mesh, upload descriptor, and upload record checks.
- `SnapshotConsumerValidation.cs` owns server snapshot tick-order checks.

Act:

- First extract JSON/file/session helpers from `octaryn_client_app.cpp`; these have narrow dependencies and can be validated with a client app build.
- Then extract input/intent/world stream helpers.
- Then split render/mesh/UI responsibilities once the data contracts are no longer private to the main app translation unit.
