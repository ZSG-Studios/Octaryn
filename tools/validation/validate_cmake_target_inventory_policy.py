#!/usr/bin/env python3

# Intentional inventory gate for active CMake targets. Add or remove entries
# when the active owner/platform/tool target graph changes.
REQUIRED_TARGETS = {
    "octaryn_shared",
    "octaryn_shared_native",
    "octaryn_shared_host_abi",
    "octaryn_native_logging",
    "octaryn_native_diagnostics",
    "octaryn_native_memory",
    "octaryn_native_profiling",
    "octaryn_native_jobs",
    "octaryn_basegame",
    "octaryn_basegame_native",
    "octaryn_basegame_bundle",
    "octaryn_server",
    "octaryn_server_bundle",
    "octaryn_server_native",
    "octaryn_server_world_time",
    "octaryn_server_block_store",
    "octaryn_server_player_simulation",
    "octaryn_server_managed_bridge",
    "octaryn_server_launch_probe",
    "octaryn_client_managed",
    "octaryn_client_native",
    "octaryn_client_asset_paths",
    "octaryn_client_app_settings",
    "octaryn_client_camera",
    "octaryn_client_camera_matrix",
    "octaryn_client_chunk_mesh_plan",
    "octaryn_client_display_catalog",
    "octaryn_client_display_menu",
    "octaryn_client_display_settings",
    "octaryn_client_frame_pacing",
    "octaryn_client_fullscreen_display_mode",
    "octaryn_client_frame_metrics",
    "octaryn_client_hidden_block_uniforms",
    "octaryn_client_host_environment",
    "octaryn_client_lighting_settings",
    "octaryn_client_render_distance",
    "octaryn_client_shader_creation",
    "octaryn_client_shader_metadata_contract",
    "octaryn_client_shaders",
    "octaryn_client_swapchain",
    "octaryn_client_visibility_flags",
    "octaryn_client_window_frame_statistics",
    "octaryn_client_window_lifecycle",
    "octaryn_client_managed_bridge",
    "octaryn_client_app",
    "octaryn_client_launch_probe",
    "octaryn_client_server_app",
    "octaryn_client_bundle",
    "octaryn_tools",
    "octaryn_shader_compiler",
    "octaryn_native_jobs_probe",
    "octaryn_client_chunk_mesh_plan_probe",
    "octaryn_client_empty_world_mesh_probe",
    "octaryn_server_world_time_probe",
    "octaryn_server_block_store_probe",
    "octaryn_server_player_simulation_probe",
    "octaryn_debug_tools",
    "octaryn_all",
    "octaryn_validate_all",
    "octaryn_validate_cmake_targets",
    "octaryn_validate_cmake_policy_separation",
    "octaryn_validate_cmake_dependency_aliases",
    "octaryn_validate_package_policy_sync",
    "octaryn_validate_project_references",
    "octaryn_validate_module_manifest_packages",
    "octaryn_validate_module_manifest_files",
    "octaryn_validate_module_manifest_probe",
    "octaryn_validate_bundle_module_payload",
    "octaryn_validate_client_server_app",
    "octaryn_client_server_app_launch_probe",
    "octaryn_validate_client_app_launch_probe",
    "octaryn_validate_client_shader_bundle",
    "octaryn_validate_module_source_api",
    "octaryn_validate_module_binary_sandbox",
    "octaryn_validate_module_layout",
    "octaryn_validate_basegame_block_catalog",
    "octaryn_validate_basegame_worldgen_content",
    "octaryn_validate_dotnet_package_assets",
    "octaryn_validate_native_abi_contracts",
    "octaryn_validate_native_owner_boundaries",
    "octaryn_validate_native_archive_format",
    "octaryn_validate_native_jobs_probe",
    "octaryn_validate_client_chunk_mesh_plan_probe",
    "octaryn_validate_client_empty_world_mesh_probe",
    "octaryn_validate_dotnet_owners",
    "octaryn_validate_world_time_probe",
    "octaryn_validate_server_world_time_native_probe",
    "octaryn_validate_server_block_store_native_probe",
    "octaryn_validate_server_player_simulation_native_probe",
    "octaryn_validate_server_persistence_probe",
    "octaryn_validate_owner_module_validation_probe",
    "octaryn_validate_server_world_blocks_probe",
    "octaryn_validate_server_world_generation_probe",
    "octaryn_validate_basegame_player_probe",
    "octaryn_validate_basegame_interaction_probe",
    "octaryn_validate_hostfxr_bridge_exports",
    "octaryn_validate_owner_launch_probes",
    "octaryn_run_client_launch_probe",
    "octaryn_run_client_app_launch_probe",
    "octaryn_run_server_launch_probe",
}

FORBIDDEN_TARGET_PATTERNS = (
    "renderdoc",
    "octaryn_engine",
    "engine_runtime",
)

REQUIRED_CMAKE_STRUCTURE = (
    "cmake/Shared/ProjectDefaults.cmake",
    "cmake/Shared/TargetArchitecture.cmake",
    "cmake/Shared/BuildOutputs.cmake",
    "cmake/Shared/OwnerBuildLayout.cmake",
    "cmake/Shared/CompilerWarnings.cmake",
    "cmake/Owners/SharedTargets.cmake",
    "cmake/Owners/BasegameTargets.cmake",
    "cmake/Owners/ServerTargets.cmake",
    "cmake/Owners/ClientTargets.cmake",
    "cmake/Owners/ClientTargets/ClientBuildPaths.cmake",
    "cmake/Owners/ClientTargets/ClientHostAppTargets.cmake",
    "cmake/Owners/ClientTargets/ClientLaunchProbeTargets.cmake",
    "cmake/Owners/ClientTargets/ClientManagedBundleTargets.cmake",
    "cmake/Owners/ClientTargets/ClientNativeLibraryTargets.cmake",
    "cmake/Owners/ClientTargets/ClientShaderTargets.cmake",
    "cmake/Owners/ToolTargets.cmake",
    "cmake/Owners/ToolTargets/ToolAggregateTargets.cmake",
    "cmake/Owners/ToolTargets/ToolBuildPaths.cmake",
    "cmake/Owners/ToolTargets/ToolBundleValidationTargets.cmake",
    "cmake/Owners/ToolTargets/ToolCmakeValidationTargets.cmake",
    "cmake/Owners/ToolTargets/ToolDebugStagingTargets.cmake",
    "cmake/Owners/ToolTargets/ToolModuleValidationTargets.cmake",
    "cmake/Owners/ToolTargets/ToolNativeTargets.cmake",
    "cmake/Owners/ToolTargets/ToolOwnerProbeValidationTargets.cmake",
    "cmake/Owners/DotNetOwner.cmake",
    "cmake/Owners/NativeOwner.cmake",
    "cmake/Dependencies/ClientDependencies.cmake",
    "cmake/Dependencies/DependencyPolicy.cmake",
    "cmake/Dependencies/DotNetHosting.cmake",
    "cmake/Dependencies/NativeDependencyAliases.cmake",
    "cmake/Dependencies/SourceDependencyCache.cmake",
    "cmake/Dependencies/ToolDependencies.cmake",
    "cmake/Platforms/PlatformDispatch.cmake",
    "cmake/Platforms/Windows/WindowsPlatform.cmake",
    "cmake/Platforms/Linux/LinuxPlatform.cmake",
    "cmake/Platforms/Linux/ArchFamily.cmake",
    "cmake/Platforms/Linux/DebianFamily.cmake",
    "cmake/Platforms/Linux/FedoraFamily.cmake",
    "cmake/Platforms/Linux/SuseFamily.cmake",
    "cmake/Toolchains/Linux/clang.cmake",
    "cmake/Toolchains/Windows/clang.cmake",
    "tools/build/tool_environment.sh",
    "tools/run_workspace_ui.sh",
    "tools/build/linux_build_environment.sh",
    "tools/build/Containerfile.arch-build",
    "tools/build/arch_packages.txt",
    "tools/build/podman_build.sh",
    "tools/profiling/tracy_tool.sh",
    "tools/build/workspace_bootstrap.sh",
    "tools/build/linux_arm64_sysroot.sh",
    "tools/ui/workspace_control_app.py",
)

FORBIDDEN_CMAKE_PATHS = (
    "cmake/Platforms/BSD",
    "cmake/Toolchains/BSD",
    "cmake/Toolchains/Windows/MinGW",
    "cmake/toolchains",
    "cmake/engine",
    "cmake/runtime",
)

FORBIDDEN_ACTIVE_WORKSPACE_PATHS = (
    "engine",
    "octaryn-engine",
    "runtime",
    "docs/validation/renderdoc.md",
    "octaryn-client/Source/Diagnostics/RenderDocCapture",
    "tools/capture/renderdoc_tool.sh",
    "tools/bootstrap",
    "tools/podman",
    "tools/setup",
    "tools/sysroots",
    "tools/tooling",
    "tools/build/podman_build.bat",
    "tools/run_workspace_ui.bat",
    "tools/setup/windows_build_environment.bat",
    "tools/setup/windows_workspace_environment.bat",
)

REQUIRED_CONFIGURE_PRESETS = (
    "debug-linux",
    "release-linux",
    "debug-windows",
    "release-windows",
)

REQUIRED_CONFIGURED_GRAPH_PRESETS = (
    "debug-linux",
    "release-linux",
    "debug-windows",
    "release-windows",
)

REQUIRED_CONFIGURE_PRESET_TOOLCHAINS = {
    "debug-linux": "${sourceDir}/cmake/Toolchains/Linux/clang.cmake",
    "release-linux": "${sourceDir}/cmake/Toolchains/Linux/clang.cmake",
    "debug-windows": "${sourceDir}/cmake/Toolchains/Windows/clang.cmake",
    "release-windows": "${sourceDir}/cmake/Toolchains/Windows/clang.cmake",
}

REQUIRED_BUILD_PRESETS = (
    "debug-linux",
    "release-linux",
    "debug-windows",
    "release-windows",
)

STATIC_ALLOWED_BUILD_ROOTS = (
    "dependencies",
)

ALLOWED_LOG_ROOTS = (
    "basegame",
    "build",
    "client",
    "server",
    "shared",
    "tools",
)

FORBIDDEN_LOG_NAME_PATTERNS = (
    "macos",
    "darwin",
    "debug-macos",
    "release-macos",
)

FORBIDDEN_BUILD_SUBROOT_NAMES = (
    "_deps",
    "cpm-cache",
    "CPM_modules",
)

FORBIDDEN_BUILD_FILE_NAMES = (
    "cpm-package-lock.cmake",
)

ALLOWED_PRESET_SUBROOTS = (
    "basegame",
    "client",
    "cmake",
    "deps",
    "server",
    "shared",
    "tools",
)

HOSTFXR_REAL_OUTPUTS_BY_PLATFORM = {
    "linux": (
        "client/native/lib/liboctaryn_client_managed_bridge.so",
        "server/native/lib/liboctaryn_server_managed_bridge.so",
        "client/native/bin/octaryn_client_launch_probe",
        "server/native/bin/octaryn_server_launch_probe",
    ),
    "windows": (
        "client/native/bin/liboctaryn_client_managed_bridge.dll",
        "server/native/bin/liboctaryn_server_managed_bridge.dll",
        "client/native/bin/octaryn_client_launch_probe.exe",
        "server/native/bin/octaryn_server_launch_probe.exe",
    ),
}

HOSTFXR_SKIP_MESSAGES = (
    "Skipping client managed bridge: .NET native hosting unavailable",
    "Skipping server managed bridge: .NET native hosting unavailable",
    "Skipping client launch probe binary: .NET native hosting unavailable",
    "Skipping server launch probe binary: .NET native hosting unavailable",
    "Skipping hostfxr bridge export validation: .NET native hosting unavailable",
    "Skipping owner launch probes: .NET native hosting unavailable",
)

REQUIRED_BUILD_COMMAND_SNIPPETS = (
    "validate_bundle_module_payload.py",
    "validate_client_server_app.py",
    "validate_client_server_app_readiness.py",
    "validate_client_shader_bundle.py",
    "--expected-manifest",
    "validate_native_owner_boundaries.py",
    "validate_native_abi_contracts.py",
    "octaryn_debug_tools",
)
