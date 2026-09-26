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
    "octaryn_character_motion",
    "octaryn_character_motion_probe",
    "octaryn_physics_probe",
    "octaryn_basegame",
    "octaryn_basegame_native",
    "octaryn_basegame_bundle",
    "octaryn_server",
    "octaryn_server_bundle",
    "octaryn_server_native",
    "octaryn_server_host",
    "octaryn_server_world_time",
    "octaryn_server_world_time_objects",
    "octaryn_server_world_time_internal",
    "octaryn_server_authority_tick",
    "octaryn_server_player_simulation",
    "octaryn_server_map_world",
    "octaryn_server_session_stream",
    "octaryn_server_world_persistence",
    "octaryn_server_managed_bridge",
    "octaryn_server_launch_probe",
    "octaryn_client_managed",
    "octaryn_client_native",
    "octaryn_client_action_audio",
    "octaryn_client_asset_paths",
    "octaryn_client_app_settings",
    "octaryn_client_camera",
    "octaryn_client_camera_matrix",
    "octaryn_client_render_backend",
    "octaryn_client_display_catalog",
    "octaryn_client_display_menu",
    "octaryn_client_display_settings",
    "octaryn_client_frame_metrics",
    "octaryn_client_frame_profile",
    "octaryn_client_fsr2",
    "octaryn_client_fullscreen_display_mode",
    "octaryn_client_function_profile",
    "octaryn_client_lighting_settings",
    "octaryn_client_local_session",
    "octaryn_client_physics",
    "octaryn_client_player_control_input",
    "octaryn_client_render_distance",
    "octaryn_client_runtime_controls",
    "octaryn_client_runtime_settings",
    "octaryn_client_slang_runtime",
    "octaryn_client_threading",
    "octaryn_client_visibility_flags",
    "octaryn_client_window_frame_statistics",
    "octaryn_client_window_lifecycle",
    "octaryn_client_world_streaming",
    "octaryn_client_shaders",
    "octaryn_client_managed_bridge",
    "octaryn_client_app",
    "octaryn_client_launch_probe",
    "octaryn_client_server_app",
    "octaryn_client_bundle",
    "octaryn_fsr2_sdk",
    "octaryn_third_party_rmlui_sdl",
    "octaryn_all",
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
    "cmake/Owners/DotNetOwner.cmake",
    "cmake/Owners/NativeOwner.cmake",
    "cmake/Dependencies/ClientDependencies.cmake",
    "cmake/Dependencies/DependencyPolicy.cmake",
    "cmake/Dependencies/DotNetHosting.cmake",
    "cmake/Dependencies/FreeType.cmake",
    "cmake/Dependencies/NativeDependencyAliases.cmake",
    "cmake/Dependencies/SourceDependencyCache.cmake",
    "cmake/Platforms/PlatformDispatch.cmake",
    "cmake/Platforms/Windows/WindowsPlatform.cmake",
    "cmake/Platforms/Linux/LinuxPlatform.cmake",
    "cmake/Toolchains/Linux/clang.cmake",
    "cmake/Toolchains/Windows/clang.cmake",
    "tools/build/linux.py",
    "tools/build/windows.py",
    "tools/build/slang-rhi.py",
    "tools/build/vsenv.py",
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
    "releases",
)

# Build-graph validator wiring was intentionally stripped (validators now run
# standalone through tools/validation); keep this empty until new wiring exists.
REQUIRED_BUILD_COMMAND_SNIPPETS = ()
