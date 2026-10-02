add_library(octaryn_character_motion STATIC
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/CharacterMotion.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/PlayerMovement.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/MeshCollisionWorld.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/MeshCollisionScene.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/MeshCharacterStep.cpp")
target_sources(octaryn_character_motion PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/SceneHullDecomposition.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/SceneBodyShapes.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/CharacterBodyPressure.cpp")
target_include_directories(octaryn_character_motion PUBLIC
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion")
target_link_libraries(octaryn_character_motion PUBLIC octaryn::deps::box3d)
target_compile_features(octaryn_character_motion PRIVATE cxx_std_20)
set_target_properties(octaryn_character_motion PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
octaryn_apply_owner_layout(octaryn_character_motion shared)

add_executable(octaryn_collision_lifetime_probe EXCLUDE_FROM_ALL
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/CollisionLifetimeProbe.cpp")
target_include_directories(octaryn_collision_lifetime_probe PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession")
target_link_libraries(octaryn_collision_lifetime_probe PRIVATE octaryn_character_motion)
target_compile_features(octaryn_collision_lifetime_probe PRIVATE cxx_std_20)
octaryn_apply_owner_layout(octaryn_collision_lifetime_probe tools)
