include_guard(GLOBAL)
include(Owners/MapImportProbeTargets)
include(Owners/GltfBufferTargets)
include(Owners/MapSceneCookTargets)
set(world_library_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/WorldLibrary")
set(world_library_map "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
set(world_library_app "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld")
set(world_library_session "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession")
octaryn_add_native_executable(octaryn_world_library_probe tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/WorldLibraryProbe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/WorldLibraryProbe/Preparation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/WorldLibraryProbe/Lazy.cpp"
        "${world_library_source}/WorldLibraryCatalog.cpp"
        "${world_library_source}/WorldLibrarySources.cpp"
        "${world_library_source}/WorldLibraryResources.cpp"
        "${world_library_source}/WorldLibrarySaves.cpp"
        "${world_library_source}/WorldLibrarySpawn.cpp"
        "${world_library_source}/WorldLibraryPreparation.cpp"
        "${world_library_source}/WorldLibraryScenePreparation.cpp"
        "${world_library_app}/MapManifest.cpp"
        "${world_library_session}/SessionFiles.cpp"
        "${world_library_map}/MapModel.cpp"
        "${world_library_map}/MapSource.cpp"
        "${world_library_map}/MapMaterials.cpp"
        "${world_library_map}/MapTextureHash.cpp"
    PUBLIC_INCLUDE_DIRS
        "${world_library_source}" "${world_library_map}" "${world_library_app}" "${world_library_session}"
    PRIVATE_LINKS octaryn_character_motion octaryn_scene_preparation octaryn::deps::glaze octaryn_gltf_buffers octaryn::deps::fastgltf octaryn::deps::meshoptimizer)
if(WIN32)
    add_custom_command(TARGET octaryn_world_library_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:octaryn_native_jobs>" "$<TARGET_FILE_DIR:octaryn_world_library_probe>"
        VERBATIM)
endif()
