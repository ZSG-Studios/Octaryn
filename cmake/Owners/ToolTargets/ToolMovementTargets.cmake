octaryn_add_native_executable(octaryn_client_movement_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientMovementProbe/main.cpp"
    PRIVATE_LINKS octaryn_client_local_session)
add_dependencies(octaryn_tools octaryn_client_movement_probe)

octaryn_add_native_executable(octaryn_client_jump_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientJumpProbe/main.cpp"
    PRIVATE_LINKS octaryn_client_local_session)
add_dependencies(octaryn_tools octaryn_client_jump_probe)

octaryn_add_native_executable(octaryn_client_block_actions_probe tools
  SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientBlockActionsProbe/main.cpp"
  PRIVATE_LINKS octaryn_client_local_session octaryn_client_world_stream
    octaryn_client_block_interaction octaryn::deps::glaze)
add_dependencies(octaryn_tools octaryn_client_block_actions_probe)

octaryn_add_native_executable(octaryn_block_prediction_qualification tools
  SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/BlockPredictionQualification/main.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/BlockPredictionQualification/BaselineCoverage.cpp"
  PRIVATE_LINKS octaryn_client_world_stream)
add_dependencies(octaryn_tools octaryn_block_prediction_qualification)
