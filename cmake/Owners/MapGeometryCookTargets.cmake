include_guard(GLOBAL)
include(Dependencies/GltfDependencies)
set(map_geometry_source "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapGeometryCook")
set(map_geometry_world "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
add_executable(octaryn_map_geometry_cook
    "${map_geometry_source}/main.cpp" "${map_geometry_source}/Simplify.cpp" "${map_geometry_source}/Test.cpp" "${map_geometry_source}/PrepareTest.cpp" "${map_geometry_source}/Tiles.cpp"
    "${map_geometry_world}/MapModel.cpp" "${map_geometry_world}/MapMaterials.cpp"
    "${map_geometry_world}/MapMeshOptimization.cpp" "${map_geometry_world}/MapLodCache.cpp"
    "${map_geometry_world}/MapTextureHash.cpp" "${map_geometry_world}/MapAssetPrepare.cpp"
    "${map_geometry_world}/MapMeshlets.cpp"
    "${map_geometry_world}/MapImages.cpp" "${map_geometry_world}/MapMipmaps.cpp" "${map_geometry_world}/MapTextureCache.cpp")
target_include_directories(octaryn_map_geometry_cook PRIVATE "${map_geometry_world}")
target_compile_features(octaryn_map_geometry_cook PRIVATE cxx_std_20)
target_link_libraries(octaryn_map_geometry_cook PRIVATE octaryn::deps::fastgltf octaryn::deps::meshoptimizer octaryn::deps::stb_image)
octaryn_owner_build_root(map_geometry_tools tools)
set_target_properties(octaryn_map_geometry_cook PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${map_geometry_tools}/map-geometry-cache")
set(octaryn_map_geometry_stamps)
set(octaryn_map_geometry_root "${client_build_root}/map-geometry")
file(GLOB map_geometry_maps CONFIGURE_DEPENDS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets/Maps/*.glb")
foreach(map IN LISTS map_geometry_maps)
    get_filename_component(map_name "${map}" NAME)
    set(stamp "${octaryn_map_geometry_root}/${map_name}.lods")
    add_custom_command(OUTPUT "${stamp}" "${stamp}.sha256"
        COMMAND "$<TARGET_FILE:octaryn_map_geometry_cook>" "${map}" "${stamp}"
        DEPENDS octaryn_map_geometry_cook "${map}" VERBATIM)
    list(APPEND octaryn_map_geometry_stamps "${stamp}" "${stamp}.sha256")
endforeach()
add_custom_target(octaryn_map_geometry DEPENDS ${octaryn_map_geometry_stamps})
