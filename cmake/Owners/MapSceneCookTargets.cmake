include_guard(GLOBAL)
include(Owners/GltfBufferTargets)
include(Owners/VirtualGeometryCookTargets)
include(Owners/SceneGeometryTargets)
include(Owners/SceneCollisionTargets)
set(scene_geometry_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/VirtualGeometry")
set(scene_map_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
set(scene_tool_source "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapSceneCook")
add_library(octaryn_scene_catalog STATIC
    "${scene_geometry_source}/SceneCatalog.cpp"
    "${scene_geometry_source}/SceneCatalogImport.cpp"
    "${scene_geometry_source}/SceneResourceHash.cpp"
    "${scene_geometry_source}/SceneOrder.cpp"
    "${scene_map_source}/MapSource.cpp"
    "${scene_map_source}/MapMaterials.cpp")
target_compile_features(octaryn_scene_catalog PUBLIC cxx_std_20)
target_include_directories(octaryn_scene_catalog PUBLIC "${scene_geometry_source}" "${scene_map_source}")
target_link_libraries(octaryn_scene_catalog PUBLIC octaryn_virtual_geometry_asset octaryn_gltf_buffers octaryn_scene_geometry octaryn::deps::fastgltf octaryn::deps::glaze)
add_library(octaryn_scene_preparation STATIC
    "${scene_geometry_source}/ScenePreparation.cpp"
    "${scene_geometry_source}/ScenePreparationCook.cpp"
    "${scene_geometry_source}/ScenePreparationBounds.cpp"
    "${scene_geometry_source}/ScenePreparationHierarchy.cpp"
    "${scene_geometry_source}/ScenePreparationLayout.cpp"
    "${scene_geometry_source}/ScenePreparationCollision.cpp"
    "${scene_geometry_source}/ScenePreparationWorldCoverage.cpp"
    "${scene_geometry_source}/SceneSpawn.cpp"
    "${scene_geometry_source}/SceneOrderLoad.cpp"
    "${scene_geometry_source}/SceneSpatialOrder.cpp"
    "${scene_geometry_source}/SceneHierarchy.cpp"
    "${scene_geometry_source}/SceneHierarchyIo.cpp"
    "${scene_geometry_source}/SceneHierarchyCook.cpp"
    "${scene_geometry_source}/SceneHierarchyPreparation.cpp"
    "${scene_geometry_source}/SceneHierarchyGeometry.cpp"
    "${scene_geometry_source}/SceneHierarchyDetail.cpp"
    "${scene_map_source}/MapSourceReader.cpp")
target_compile_features(octaryn_scene_preparation PUBLIC cxx_std_20)
target_link_libraries(octaryn_scene_preparation PUBLIC octaryn_scene_catalog octaryn_scene_geometry PRIVATE octaryn_scene_collision octaryn_virtual_geometry_residency)
octaryn_owner_build_root(scene_preparation_build client)
set_target_properties(octaryn_scene_preparation PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${scene_preparation_build}/native/lib")
add_executable(octaryn_map_scene_cook
    "${scene_tool_source}/main.cpp"
    "${scene_tool_source}/SceneCook.cpp"
    "${scene_tool_source}/SceneBounds.cpp"
    "${scene_tool_source}/SceneNeighborhood.cpp"
    "${scene_tool_source}/PreparationTest.cpp"
    "${scene_tool_source}/OrderTest.cpp"
    "${scene_tool_source}/Test.cpp"
    "${scene_tool_source}/TransformTest.cpp")
target_sources(octaryn_map_scene_cook PRIVATE "${scene_tool_source}/Hierarchy.cpp" "${scene_tool_source}/HierarchyTest.cpp")
target_sources(octaryn_map_scene_cook PRIVATE "${scene_tool_source}/HierarchyAudit.cpp")
target_sources(octaryn_map_scene_cook PRIVATE "${scene_tool_source}/CoarseFacesTest.cpp")
target_sources(octaryn_map_scene_cook PRIVATE "${scene_tool_source}/WorldPreparationTest.cpp")
target_compile_features(octaryn_map_scene_cook PRIVATE cxx_std_20)
target_link_libraries(octaryn_map_scene_cook PRIVATE octaryn_scene_preparation)
octaryn_owner_build_root(scene_tool_build tools)
set_target_properties(octaryn_map_scene_cook PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${scene_tool_build}/map-scene-cook")
if(WIN32)
    add_custom_command(TARGET octaryn_map_scene_cook POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:octaryn_native_jobs>" "$<TARGET_FILE_DIR:octaryn_map_scene_cook>"
        VERBATIM)
endif()
add_executable(octaryn_scene_catalog_probe "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/SceneResidencyProbe/Catalog.cpp")
target_link_libraries(octaryn_scene_catalog_probe PRIVATE octaryn_scene_catalog octaryn_scene_geometry)
set_target_properties(octaryn_scene_catalog_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${scene_tool_build}/scene-residency-probe")
