include_guard(GLOBAL)
include(Dependencies/GltfDependencies)
octaryn_fetch_header_dependency(bc7enc_rdo bc7enc_source_dir
    GITHUB_REPOSITORY ${OCTARYN_DEP_bc7enc_rdo_REPOSITORY}
    GIT_TAG ${OCTARYN_DEP_bc7enc_rdo_TAG})
if(NOT bc7enc_source_dir)
    message(FATAL_ERROR "Offline map textures require pinned bc7enc_rdo sources.")
endif()
set(map_cook_source "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapTextureCook")
set(map_world_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
add_executable(octaryn_map_texture_cook
    "${map_cook_source}/main.cpp" "${map_cook_source}/Encode.cpp" "${map_cook_source}/Test.cpp" "${map_cook_source}/Compare.cpp"
    "${map_world_source}/MapModel.cpp" "${map_world_source}/MapMaterials.cpp"
    "${map_world_source}/MapImages.cpp" "${map_world_source}/MapMipmaps.cpp"
    "${map_world_source}/MapTextureCache.cpp" "${map_world_source}/MapTextureHash.cpp"
    "${bc7enc_source_dir}/bc7enc.cpp" "${bc7enc_source_dir}/bc7decomp.cpp")
target_include_directories(octaryn_map_texture_cook PRIVATE "${map_world_source}" "${bc7enc_source_dir}")
target_compile_features(octaryn_map_texture_cook PRIVATE cxx_std_20)
target_link_libraries(octaryn_map_texture_cook PRIVATE octaryn::deps::fastgltf octaryn::deps::stb_image Threads::Threads)
octaryn_owner_build_root(map_tools_root tools)
set_target_properties(octaryn_map_texture_cook PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${map_tools_root}/map-texture-cache")
set(octaryn_map_cook_stamps)
set(octaryn_map_cache_root "${client_build_root}/map-textures")
file(GLOB map_cook_maps CONFIGURE_DEPENDS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets/Maps/*.glb")
foreach(map IN LISTS map_cook_maps)
    get_filename_component(map_name "${map}" NAME)
    set(stamp "${octaryn_map_cache_root}/${map_name}.textures/map-texture-cook.json")
    add_custom_command(OUTPUT "${stamp}"
        COMMAND "$<TARGET_FILE:octaryn_map_texture_cook>" "${map}" "${octaryn_map_cache_root}/${map_name}.textures"
        DEPENDS octaryn_map_texture_cook "${map}"
        VERBATIM)
    list(APPEND octaryn_map_cook_stamps "${stamp}")
endforeach()
add_custom_target(octaryn_map_textures DEPENDS ${octaryn_map_cook_stamps})
