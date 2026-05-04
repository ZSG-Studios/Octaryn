set(octaryn_workspace_control_files
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control_app.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/__init__.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/commands.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/layout.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/logs.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/paths.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/process_controller.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/status.py"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/window.py")

set(octaryn_debug_tool_files
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/run_workspace_ui.sh"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/podman_build.sh"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/linux_build_environment.sh"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/Containerfile.arch-build"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/arch_packages.txt"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/tool_environment.sh"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/profiling/tracy_tool.sh"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/workspace_bootstrap.sh"
    ${octaryn_workspace_control_files})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(octaryn_debug_tool_build_commands
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${octaryn_debug_tool_root}/build"
            "${octaryn_debug_tool_root}/profiling"
            "${octaryn_debug_tool_root}/ui"
            "${octaryn_debug_tool_root}/ui/workspace_control"
            "${octaryn_debug_tool_log_root}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/run_workspace_ui.sh"
            "${octaryn_debug_tool_root}/run_workspace_ui.sh"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/podman_build.sh"
            "${octaryn_debug_tool_root}/build/podman_build.sh"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/linux_build_environment.sh"
            "${octaryn_debug_tool_root}/build/linux_build_environment.sh"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/Containerfile.arch-build"
            "${octaryn_debug_tool_root}/build/Containerfile.arch-build"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/arch_packages.txt"
            "${octaryn_debug_tool_root}/build/arch_packages.txt"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/tool_environment.sh"
            "${octaryn_debug_tool_root}/build/tool_environment.sh"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/profiling/tracy_tool.sh"
            "${octaryn_debug_tool_root}/profiling/tracy_tool.sh"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/build/workspace_bootstrap.sh"
            "${octaryn_debug_tool_root}/build/workspace_bootstrap.sh"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control_app.py"
            "${octaryn_debug_tool_root}/ui/workspace_control_app.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/__init__.py"
            "${octaryn_debug_tool_root}/ui/workspace_control/__init__.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/commands.py"
            "${octaryn_debug_tool_root}/ui/workspace_control/commands.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/layout.py"
            "${octaryn_debug_tool_root}/ui/workspace_control/layout.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/logs.py"
            "${octaryn_debug_tool_root}/ui/workspace_control/logs.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/paths.py"
            "${octaryn_debug_tool_root}/ui/workspace_control/paths.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/process_controller.py"
            "${octaryn_debug_tool_root}/ui/workspace_control/process_controller.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/status.py"
            "${octaryn_debug_tool_root}/ui/workspace_control/status.py"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/ui/workspace_control/window.py"
            "${octaryn_debug_tool_root}/ui/workspace_control/window.py"
        COMMAND chmod 755
            "${octaryn_debug_tool_root}/run_workspace_ui.sh"
            "${octaryn_debug_tool_root}/build/podman_build.sh"
            "${octaryn_debug_tool_root}/build/linux_build_environment.sh"
            "${octaryn_debug_tool_root}/build/tool_environment.sh"
            "${octaryn_debug_tool_root}/profiling/tracy_tool.sh"
            "${octaryn_debug_tool_root}/build/workspace_bootstrap.sh"
            "${octaryn_debug_tool_root}/ui/workspace_control_app.py"
        COMMAND "${CMAKE_COMMAND}" -E env "OCTARYN_WORKSPACE_ROOT=${OCTARYN_WORKSPACE_ROOT_DIR}"
            "${octaryn_debug_tool_root}/profiling/tracy_tool.sh"
            --preset "${OCTARYN_BUILD_PRESET_NAME}"
            build
    )

    add_custom_target(octaryn_debug_tools
        ${octaryn_debug_tool_build_commands}
        DEPENDS
            ${octaryn_debug_tool_files}
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
        VERBATIM)
else()
    add_custom_target(octaryn_debug_tools
        COMMAND "${CMAKE_COMMAND}" -E echo "Skipping debug tool staging for ${OCTARYN_BUILD_PRESET_NAME}: debug tools ship with Debug presets."
        VERBATIM)
endif()
