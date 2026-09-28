include_guard(GLOBAL)
include(Owners/VirtualGeometryCookTargets)
include(Owners/VirtualGeometryResidencyTargets)
set(geometry_stream_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/VirtualGeometry")
add_library(octaryn_virtual_geometry_stream STATIC
    "${geometry_stream_source}/GeometryStream.cpp"
    "${geometry_stream_source}/GeometryStreamJobs.cpp")
target_compile_features(octaryn_virtual_geometry_stream PUBLIC cxx_std_20)
target_include_directories(octaryn_virtual_geometry_stream PUBLIC "${geometry_stream_source}")
target_link_libraries(octaryn_virtual_geometry_stream PUBLIC octaryn_virtual_geometry_asset
    octaryn_virtual_geometry_residency octaryn_native_jobs octaryn::deps::slang_rhi)
add_executable(octaryn_virtual_geometry_stream_probe
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/VirtualGeometryCook/StreamProbe.cpp")
target_link_libraries(octaryn_virtual_geometry_stream_probe PRIVATE octaryn_virtual_geometry_stream)
target_include_directories(octaryn_virtual_geometry_stream_probe PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
octaryn_owner_build_root(geometry_stream_tools tools)
set_target_properties(octaryn_virtual_geometry_stream_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${geometry_stream_tools}/virtual-geometry")
