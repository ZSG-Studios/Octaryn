include_guard(GLOBAL)

set(octaryn_server_world_time_includes
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Time")
add_library(octaryn_server_world_time_objects OBJECT
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Time/Clock.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Time/WorldTimeIntent.cpp")
octaryn_apply_owner_layout(octaryn_server_world_time_objects server)
octaryn_enable_default_warnings(octaryn_server_world_time_objects)
set_target_properties(octaryn_server_world_time_objects PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(octaryn_server_world_time_objects
    PRIVATE ${octaryn_server_world_time_includes})
target_link_libraries(octaryn_server_world_time_objects PRIVATE octaryn::deps::glaze)

# Native probes inspect internals; managed hosts use the explicit C ABI DLL.
octaryn_add_native_static_library(
    octaryn_server_world_time_internal
    server
    SOURCES $<TARGET_OBJECTS:octaryn_server_world_time_objects>
    PUBLIC_INCLUDE_DIRS ${octaryn_server_world_time_includes}
    PRIVATE_LINKS octaryn::deps::glaze)
octaryn_add_native_shared_library(
    octaryn_server_world_time
    server
    SOURCES $<TARGET_OBJECTS:octaryn_server_world_time_objects>
    PUBLIC_INCLUDE_DIRS ${octaryn_server_world_time_includes}
    PRIVATE_LINKS octaryn::deps::glaze)
