target_sources(octaryn_client_voxel_tracing PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing/FarField.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing/FarFieldGenerator.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelTracing/FarFieldResident.cpp")
target_include_directories(octaryn_client_voxel_tracing PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldStream"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Source/Gameplay/Terrain")
