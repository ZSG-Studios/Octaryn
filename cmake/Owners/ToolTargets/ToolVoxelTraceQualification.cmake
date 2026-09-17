octaryn_add_native_executable(
    octaryn_voxel_trace_qualification
    tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/voxel_trace_world_test.cpp"
    PRIVATE_LINKS octaryn_client_voxel_tracing)
set_target_properties(octaryn_voxel_trace_qualification PROPERTIES EXCLUDE_FROM_ALL TRUE)
