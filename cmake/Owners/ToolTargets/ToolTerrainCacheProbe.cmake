octaryn_add_native_executable(octaryn_terrain_cache_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/TerrainCacheProbe/main.cpp"
    PUBLIC_INCLUDE_DIRS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Source/Gameplay/Terrain"
    PRIVATE_LINKS octaryn_client_world_stream)
set_target_properties(octaryn_terrain_cache_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(octaryn_validate_terrain_cache
    COMMAND "$<TARGET_FILE:octaryn_terrain_cache_probe>"
    DEPENDS octaryn_terrain_cache_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
