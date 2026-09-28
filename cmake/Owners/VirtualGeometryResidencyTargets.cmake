include_guard(GLOBAL)
set(geometry_residency_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/VirtualGeometry")
add_library(octaryn_virtual_geometry_residency STATIC
    "${geometry_residency_source}/PageResidency.cpp"
    "${geometry_residency_source}/Selection.cpp")
target_compile_features(octaryn_virtual_geometry_residency PUBLIC cxx_std_20)
target_include_directories(octaryn_virtual_geometry_residency PUBLIC "${geometry_residency_source}")
add_executable(octaryn_virtual_geometry_residency_probe
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/VirtualGeometryProbe/ResidencyProbe.cpp")
target_link_libraries(octaryn_virtual_geometry_residency_probe PRIVATE octaryn_virtual_geometry_residency)
octaryn_owner_build_root(geometry_residency_tools tools)
set_target_properties(octaryn_virtual_geometry_residency_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${geometry_residency_tools}/virtual-geometry")
