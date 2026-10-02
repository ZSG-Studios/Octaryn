include_guard(GLOBAL)
include(Owners/GltfBufferTargets)

set(scene_loading_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/SceneLoading")
octaryn_add_native_shared_library(octaryn_scene_loading shared
    SOURCES
        "${scene_loading_source}/SceneLoading.cpp"
        "${scene_loading_source}/ScenePreparation.cpp"
        "${scene_loading_source}/SceneVerification.cpp"
        "${scene_loading_source}/GltfMetadata.cpp"
        "${scene_loading_source}/CatalogMetadata.cpp"
        "${scene_loading_source}/SceneCookIdentity.cpp"
    PUBLIC_INCLUDE_DIRS "${scene_loading_source}"
    PRIVATE_LINKS octaryn_native_jobs octaryn_content_digest octaryn_gltf_buffers
        octaryn::deps::fastgltf octaryn::deps::glaze)
target_compile_definitions(octaryn_scene_loading PRIVATE OCTARYN_SCENE_LOADING_BUILD)
add_dependencies(octaryn_shared_native octaryn_scene_loading)

octaryn_add_native_executable(octaryn_scene_loading_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/SceneLoadingProbe/main.cpp"
    PRIVATE_LINKS octaryn_scene_loading octaryn_native_jobs octaryn_content_digest)
add_dependencies(octaryn_shared_native octaryn_scene_loading_probe)
add_custom_command(TARGET octaryn_scene_loading_probe POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_scene_loading>" "$<TARGET_FILE_DIR:octaryn_scene_loading_probe>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_native_jobs>" "$<TARGET_FILE_DIR:octaryn_scene_loading_probe>"
    VERBATIM)
