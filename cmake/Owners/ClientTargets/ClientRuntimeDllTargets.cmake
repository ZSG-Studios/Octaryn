set(octaryn_client_runtime_bundle_commands)
set(octaryn_client_runtime_bundle_outputs)
set(octaryn_client_runtime_files)
if(OCTARYN_CLIENT_SLANG_RUNTIME_FILES)
    list(APPEND octaryn_client_runtime_files ${OCTARYN_CLIENT_SLANG_RUNTIME_FILES})
    if(OCTARYN_DOTNET_HOSTING_AVAILABLE)
        list(APPEND octaryn_client_runtime_files "${OCTARYN_DOTNET_NETHOST_RUNTIME}")
    endif()
    foreach(runtime_file IN LISTS octaryn_client_runtime_files)
        get_filename_component(runtime_name "${runtime_file}" NAME)
        list(APPEND octaryn_client_runtime_bundle_outputs "${octaryn_client_bundle_dir}/${runtime_name}")
        list(APPEND octaryn_client_runtime_bundle_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_file}" "${octaryn_client_bundle_stage_dir}/${runtime_name}")
    endforeach()

    set(octaryn_client_slang_runtime_outputs)
    set(octaryn_client_stale_gfx_outputs)
    foreach(owner client tools)
        octaryn_owner_native_root(runtime_native_root "${owner}")
        if(OCTARYN_TARGET_PLATFORM STREQUAL "Windows")
            list(APPEND octaryn_client_stale_gfx_outputs "${runtime_native_root}/bin/gfx.dll")
        endif()
        foreach(runtime_file IN LISTS OCTARYN_CLIENT_SLANG_RUNTIME_FILES)
            get_filename_component(runtime_name "${runtime_file}" NAME)
            set(runtime_output "${runtime_native_root}/bin/${runtime_name}")
            add_custom_command(OUTPUT "${runtime_output}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${runtime_native_root}/bin"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${runtime_file}" "${runtime_output}"
                DEPENDS "${runtime_file}"
                VERBATIM)
            list(APPEND octaryn_client_slang_runtime_outputs "${runtime_output}")
        endforeach()
    endforeach()
    if(octaryn_client_slang_runtime_outputs)
        set(stale_cleanup)
        if(octaryn_client_stale_gfx_outputs)
            set(stale_cleanup COMMAND "${CMAKE_COMMAND}" -E rm -f ${octaryn_client_stale_gfx_outputs})
        endif()
        add_custom_target(octaryn_client_slang_runtime
            ${stale_cleanup}
            DEPENDS ${octaryn_client_slang_runtime_outputs}
            VERBATIM)
        add_dependencies(octaryn_client_render_backend octaryn_client_slang_runtime)
    endif()
endif()
