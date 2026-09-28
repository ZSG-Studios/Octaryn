include_guard(GLOBAL)
include(Dependencies/GltfDependencies)
set(virtual_geometry_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/VirtualGeometry")
set(virtual_geometry_map "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
set(virtual_geometry_tools "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/VirtualGeometryCook")
get_target_property(virtual_geometry_meshoptimizer meshoptimizer SOURCE_DIR)
if(NOT EXISTS "${virtual_geometry_meshoptimizer}/demo/clusterlod.h")
    message(FATAL_ERROR "Virtual geometry requires clusterlod.h from registry-pinned meshoptimizer.")
endif()
add_library(octaryn_virtual_geometry_asset STATIC
    "${virtual_geometry_source}/GeometryFormat.cpp"
    "${virtual_geometry_source}/GeometryCache.cpp"
    "${virtual_geometry_source}/GeometryCook.cpp"
    "${virtual_geometry_source}/ClusterLod.cpp"
    "${virtual_geometry_map}/MapTextureHash.cpp")
target_include_directories(octaryn_virtual_geometry_asset PUBLIC "${virtual_geometry_source}")
target_include_directories(octaryn_virtual_geometry_asset SYSTEM PRIVATE "${virtual_geometry_meshoptimizer}/demo")
target_compile_features(octaryn_virtual_geometry_asset PUBLIC cxx_std_20)
target_link_libraries(octaryn_virtual_geometry_asset PUBLIC octaryn::deps::meshoptimizer)
add_executable(octaryn_virtual_geometry_cook
    "${virtual_geometry_tools}/main.cpp" "${virtual_geometry_tools}/Test.cpp"
    "${virtual_geometry_map}/MapModel.cpp" "${virtual_geometry_map}/MapMaterials.cpp")
target_include_directories(octaryn_virtual_geometry_cook PRIVATE "${virtual_geometry_map}")
target_link_libraries(octaryn_virtual_geometry_cook PRIVATE octaryn_virtual_geometry_asset octaryn::deps::fastgltf)
octaryn_owner_build_root(virtual_geometry_build tools)
set_target_properties(octaryn_virtual_geometry_cook PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${virtual_geometry_build}/virtual-geometry-cook")
set_target_properties(octaryn_virtual_geometry_asset PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${virtual_geometry_build}/virtual-geometry-cook")
