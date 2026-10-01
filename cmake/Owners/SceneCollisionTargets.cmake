include_guard(GLOBAL)
include(Owners/GltfBufferTargets)
include(Owners/SceneGeometryTargets)
set(scene_collision_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/SceneCollision")
octaryn_add_native_static_library(octaryn_scene_collision shared
    SOURCES "${scene_collision_source}/SceneCollisionCatalog.cpp" "${scene_collision_source}/SceneCollisionResidency.cpp"
        "${scene_collision_source}/SceneCollisionJobs.cpp"
    PUBLIC_INCLUDE_DIRS "${scene_collision_source}"
    PRIVATE_LINKS octaryn_character_motion octaryn_gltf_buffers octaryn_scene_geometry octaryn_native_jobs octaryn::deps::glaze)
target_link_libraries(octaryn_scene_collision PUBLIC octaryn_character_motion)
