include_guard(GLOBAL)

include(Dependencies/SourceDependencyCache)

# Exact client map indexing/cache/fetch optimization; no simplification or LOD.
if(NOT TARGET octaryn::deps::meshoptimizer)
    octaryn_add_dependency_wrapper(octaryn_client_meshoptimizer octaryn::deps::meshoptimizer)
    octaryn_fetch_source_dependency(
        meshoptimizer
        GITHUB_REPOSITORY ${OCTARYN_DEP_meshoptimizer_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_meshoptimizer_TAG}
        OPTIONS ${OCTARYN_DEP_meshoptimizer_OPTIONS})
    octaryn_link_first_available_dependency(octaryn_client_meshoptimizer meshoptimizer_available meshoptimizer)
    if(NOT meshoptimizer_available)
        message(FATAL_ERROR "Map rendering requires the pinned meshoptimizer dependency.")
    endif()
endif()

# fastgltf backs both the client GLB presentation and the server map world.
if(NOT TARGET octaryn::deps::fastgltf)
    octaryn_add_dependency_wrapper(octaryn_client_fastgltf octaryn::deps::fastgltf)
    octaryn_fetch_source_dependency(
        fastgltf
        GITHUB_REPOSITORY ${OCTARYN_DEP_fastgltf_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_fastgltf_TAG}
        OPTIONS ${OCTARYN_DEP_fastgltf_OPTIONS})
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
        GITHUB_REPOSITORY ${OCTARYN_DEP_stb_REPOSITORY}
        GIT_TAG ${OCTARYN_DEP_stb_TAG})
    if(stb_source_dir)
        target_include_directories(octaryn_client_stb_image SYSTEM INTERFACE "${stb_source_dir}")
    else()
        message(FATAL_ERROR "The map world requires the pinned stb image headers.")
    endif()
endif()
