include_guard(GLOBAL)
octaryn_add_native_shared_library(
    octaryn_server_world_items
    server
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Items/WorldItems.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Items"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/World/Items")
target_compile_definitions(octaryn_server_world_items PRIVATE OCTARYN_WORLD_ITEMS_EXPORTS)
add_dependencies(octaryn_server_native octaryn_server_world_items)

octaryn_add_native_static_library(
    octaryn_client_world_items
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldItems/WorldItemsClient.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldItems/ItemFiles.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldItems"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/World/Items")
target_include_directories(octaryn_client_world_items PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession")
