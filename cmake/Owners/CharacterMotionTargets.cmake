add_library(octaryn_character_motion STATIC
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/CharacterMotion.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/PlayerMovement.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/PlayerJoltMovement.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/PlayerJoltWorld.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion/PlayerJoltMesh.cpp")
target_include_directories(octaryn_character_motion PUBLIC
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/CharacterMotion")
target_link_libraries(octaryn_character_motion PRIVATE octaryn::deps::jolt)
target_compile_features(octaryn_character_motion PRIVATE cxx_std_20)
set_target_properties(octaryn_character_motion PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
octaryn_apply_owner_layout(octaryn_character_motion shared)
