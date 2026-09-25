octaryn_add_native_executable(octaryn_server_world_items_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldItemsProbe/main.cpp"
    PRIVATE_LINKS octaryn_server_world_items)
add_custom_target(octaryn_validate_server_world_items
    COMMAND "${CMAKE_COMMAND}" -E env --modify
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:octaryn_server_world_items>" --
        "$<TARGET_FILE:octaryn_server_world_items_probe>"
    DEPENDS octaryn_server_world_items_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)
target_sources(octaryn_client_draw_preparation_probe PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientDrawPreparationProbe/TemporalValidation.cpp")

octaryn_add_native_executable(octaryn_client_world_items_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldItemsProbe/main.cpp"
    PRIVATE_LINKS octaryn_client_world_items)
add_custom_target(octaryn_validate_client_world_items
    COMMAND "$<TARGET_FILE:octaryn_client_world_items_probe>"
        "${OCTARYN_BUILD_PRESET_ROOT}/tools/validation/world-items-client"
    DEPENDS octaryn_client_world_items_probe VERBATIM)
