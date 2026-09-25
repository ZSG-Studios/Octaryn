include_guard(GLOBAL)

function(octaryn_owner_build_root output_var owner)
    set(${output_var} "${OCTARYN_BUILD_PRESET_ROOT}/${owner}" PARENT_SCOPE)
endfunction()

function(octaryn_owner_native_root output_var owner)
    octaryn_owner_build_root(owner_build_root "${owner}")
    set(${output_var} "${owner_build_root}/native" PARENT_SCOPE)
endfunction()

function(octaryn_owner_managed_root output_var owner)
    octaryn_owner_build_root(owner_build_root "${owner}")
    set(${output_var} "${owner_build_root}/managed" PARENT_SCOPE)
endfunction()

function(octaryn_owner_managed_obj_root output_var owner)
    octaryn_owner_build_root(owner_build_root "${owner}")
    set(${output_var} "${owner_build_root}/managed-obj" PARENT_SCOPE)
endfunction()

function(octaryn_owner_log_root output_var owner)
    set(${output_var} "${OCTARYN_LOG_ROOT}/${owner}" PARENT_SCOPE)
endfunction()

function(octaryn_apply_owner_layout target_name owner)
    octaryn_owner_build_root(owner_build_root "${owner}")
    octaryn_owner_native_root(owner_native_root "${owner}")
    octaryn_owner_log_root(owner_log_root "${owner}")

    file(MAKE_DIRECTORY
        "${owner_native_root}/bin"
        "${owner_native_root}/lib"
        "${owner_build_root}/generated"
        "${owner_log_root}")

    set_target_properties(${target_name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${owner_native_root}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${owner_native_root}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${owner_native_root}/bin")
    if(OCTARYN_TARGET_PLATFORM STREQUAL "Linux")
        set_target_properties(${target_name} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH_USE_LINK_PATH FALSE
            INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib;$ORIGIN/../../../shared/native/lib;$ORIGIN/../../../server/native/lib;$ORIGIN/../../../client/native/lib;$ORIGIN/../../../client/native/bin")
    endif()
endfunction()
