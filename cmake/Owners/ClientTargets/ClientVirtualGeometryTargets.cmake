include_guard(GLOBAL)
set(geometry_runtime "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/VirtualGeometry")
target_sources(octaryn_client_render_backend PRIVATE
    "${geometry_runtime}/SelectionGpu.cpp"
    "${geometry_runtime}/HybridRenderer.cpp"
    "${geometry_runtime}/RayGeometry.cpp"
    "${geometry_runtime}/RayGeometryBuild.cpp"
    "${geometry_runtime}/RayGeometryCompaction.cpp"
    "${geometry_runtime}/WorldGeometry.cpp")
target_include_directories(octaryn_client_render_backend PUBLIC "${geometry_runtime}")
target_link_libraries(octaryn_client_render_backend PRIVATE
    octaryn_virtual_geometry_residency octaryn_virtual_geometry_stream)
