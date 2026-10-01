add_library(octaryn_item_motion STATIC
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/ItemMotion/ItemMotion.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/ItemMotion/MeshItemStep.cpp")
target_include_directories(octaryn_item_motion PUBLIC
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/ItemMotion")
target_link_libraries(octaryn_item_motion PUBLIC octaryn_character_motion)
target_compile_features(octaryn_item_motion PRIVATE cxx_std_20)
set_target_properties(octaryn_item_motion PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
octaryn_apply_owner_layout(octaryn_item_motion shared)

add_executable(octaryn_item_motion_scale_probe EXCLUDE_FROM_ALL
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/ItemMotionScaleProbe.cpp")
target_link_libraries(octaryn_item_motion_scale_probe PRIVATE octaryn_item_motion)
target_compile_features(octaryn_item_motion_scale_probe PRIVATE cxx_std_20)
octaryn_apply_owner_layout(octaryn_item_motion_scale_probe tools)
