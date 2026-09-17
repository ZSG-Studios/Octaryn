octaryn_add_native_static_library(
    octaryn_client_voxel_tracing
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing/VoxelTraceWorld.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing/VoxelTraceChunk.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing/VoxelTracePreparation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing/VoxelTraceUploadPlan.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing/VoxelTraceUpload.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing"
    PRIVATE_LINKS octaryn::deps::slang_rhi)
add_dependencies(octaryn_client_native octaryn_client_voxel_tracing)
