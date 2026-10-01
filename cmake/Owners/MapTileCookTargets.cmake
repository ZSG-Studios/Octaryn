include_guard(GLOBAL)
include(Owners/GltfBufferTargets)
include(Dependencies/GltfDependencies)
set(map_tile_source "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapTileCook")
set(map_tile_world "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
add_executable(octaryn_map_tile_cook
    "${map_tile_source}/main.cpp" "${map_tile_source}/Materials.cpp" "${map_tile_source}/Textures.cpp"
    "${map_tile_source}/Write.cpp" "${map_tile_source}/Partition.cpp" "${map_tile_source}/Test.cpp" "${map_tile_source}/Verify.cpp"
    "${map_tile_source}/Order.cpp" "${map_tile_source}/Compare.cpp" "${map_tile_source}/OrderTest.cpp"
    "${map_tile_world}/MapModel.cpp"
    "${map_tile_world}/MapSource.cpp" "${map_tile_world}/MapMaterials.cpp" "${map_tile_world}/MapTextureHash.cpp"
    "${map_tile_world}/MapMipmaps.cpp" "${map_tile_world}/MapTextureCache.cpp")
target_include_directories(octaryn_map_tile_cook PRIVATE "${map_tile_world}")
target_compile_features(octaryn_map_tile_cook PRIVATE cxx_std_20)
target_link_libraries(octaryn_map_tile_cook PRIVATE octaryn_gltf_buffers octaryn::deps::fastgltf octaryn::deps::meshoptimizer)
octaryn_owner_build_root(map_tile_tools tools)
set_target_properties(octaryn_map_tile_cook PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${map_tile_tools}/map-tiles")

# HQ200 has a separate tiled manifest; the monolithic reference remains bundled.
set(map_tile_input "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets/Maps/main.glb")
set(map_tile_manifest "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets/Maps/map.json")
if(EXISTS "${map_tile_input}" AND EXISTS "${map_tile_manifest}")
    file(READ "${map_tile_manifest}" map_tile_view)
    foreach(component RANGE 0 2)
        string(JSON map_tile_spawn_${component} GET "${map_tile_view}" spawn ${component})
    endforeach()
    string(JSON map_tile_yaw GET "${map_tile_view}" yaw)
    string(JSON map_tile_pitch GET "${map_tile_view}" pitch)
    set(map_tile_output "${client_build_root}/map-tiles/bistro-hq200")
    add_custom_command(OUTPUT "${map_tile_output}/complete.stamp"
        COMMAND "$<TARGET_FILE:octaryn_map_tile_cook>" "${map_tile_input}"
            "${octaryn_map_cache_root}/main.glb.textures" "${map_tile_output}"
            "${map_tile_spawn_0}" "${map_tile_spawn_1}" "${map_tile_spawn_2}" "${map_tile_yaw}" "${map_tile_pitch}"
        COMMAND "${CMAKE_COMMAND}" -E env "OCTARYN_CLIENT_PERFORMANCE_PROFILE=HQ200"
            "$<TARGET_FILE:octaryn_map_geometry_cook>" --tiles "${map_tile_output}"
        COMMAND "$<TARGET_FILE:octaryn_map_tile_cook>" --verify "${map_tile_input}" "${map_tile_output}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${map_tile_output}/complete.stamp"
        DEPENDS octaryn_map_tile_cook octaryn_map_geometry_cook octaryn_map_textures "${map_tile_input}" "${map_tile_manifest}"
        VERBATIM)
    add_custom_target(octaryn_map_tiles DEPENDS "${map_tile_output}/complete.stamp")
    set(octaryn_map_tile_stage "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Tools/MapImport/StageMapTiles.py")
    set(octaryn_map_tile_bundle_depends octaryn_map_tiles "${map_tile_output}/complete.stamp" "${octaryn_map_tile_stage}")
    set(octaryn_map_tile_bundle_commands
        COMMAND "${Python3_EXECUTABLE}" "${octaryn_map_tile_stage}"
            --source-tiles "${map_tile_output}" --source-map "${map_tile_input}"
            --bundle-maps "${octaryn_client_bundle_stage_dir}/Client/Assets/Maps")
endif()
