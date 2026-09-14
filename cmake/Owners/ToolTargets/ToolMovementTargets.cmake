octaryn_add_native_executable(octaryn_client_movement_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientMovementProbe/main.cpp"
    PRIVATE_LINKS octaryn_client_local_session)
add_dependencies(octaryn_tools octaryn_client_movement_probe)
