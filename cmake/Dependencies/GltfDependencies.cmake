include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

# fastgltf backs both the client GLB presentation and the server map world.
if(NOT TARGET octaryn::deps::fastgltf)
    octaryn_add_dependency_wrapper(octaryn_client_fastgltf octaryn::deps::fastgltf)
    octaryn_fetch_source_dependency(
        fastgltf
        GITHUB_REPOSITORY spnda/fastgltf
        GIT_TAG v0.9.0
        OPTIONS
            "FASTGLTF_DOWNLOAD_SIMDJSON OFF"
            "FASTGLTF_TESTS OFF")
    octaryn_link_first_available_dependency(octaryn_client_fastgltf fastgltf_available fastgltf::fastgltf)
    if(TARGET fastgltf)
        get_target_property(fastgltf_includes fastgltf INTERFACE_INCLUDE_DIRECTORIES)
        if(fastgltf_includes)
            set_target_properties(fastgltf PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${fastgltf_includes}")
        endif()
    endif()
endif()

# stb_image decodes embedded/external map PNG/JPEG payloads to RGBA8.
if(NOT TARGET octaryn::deps::stb_image)
    octaryn_add_dependency_wrapper(octaryn_client_stb_image octaryn::deps::stb_image)
    octaryn_fetch_header_dependency(
        stb
        stb_source_dir
        GITHUB_REPOSITORY nothings/stb
        GIT_TAG master)
    if(stb_source_dir)
        target_include_directories(octaryn_client_stb_image SYSTEM INTERFACE "${stb_source_dir}")
    else()
        message(FATAL_ERROR "The map world requires the pinned stb image headers.")
    endif()
endif()
