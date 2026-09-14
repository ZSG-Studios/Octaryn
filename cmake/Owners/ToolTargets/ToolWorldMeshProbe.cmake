# Headless production GPU meshing and G-buffer parity. Never part of automatic app startup.
octaryn_add_native_executable(
    octaryn_client_world_mesh_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Fixture.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/AtlasFiltering.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/AtlasMipProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/AtlasAnimationProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Oracle.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/GreedyOutput.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/GreedyOutputTiming.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/FluidOracle.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Raster.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Sampling.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/SamplingReference.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Edges.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Bindings.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Batch.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Frames.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Culling.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/HaloLifecycle.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/RelativePrecision.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/Seams.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/PatchCoordinates.cpp"
    PRIVATE_LINKS octaryn_client_render_backend octaryn_client_lighting_settings
        octaryn_client_asset_paths octaryn::deps::glaze octaryn::deps::slang_rhi octaryn::deps::sdl3)
set_target_properties(octaryn_client_world_mesh_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
set(world_mesh_fixture "${OCTARYN_BUILD_PRESET_ROOT}/tools/validation/world-mesh")
set(world_mesh_runtime_commands)
set(world_mesh_validation_environment)
set(world_mesh_validation_inputs)
if(WIN32)
    foreach(runtime_file IN LISTS OCTARYN_CLIENT_SLANG_RUNTIME_FILES)
        list(APPEND world_mesh_runtime_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${runtime_file}" "${world_mesh_fixture}/")
    endforeach()
    list(APPEND world_mesh_validation_environment
        "VK_LAYER_PATH=${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation"
        "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
        "VK_LAYER_SETTINGS_PATH=${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation")
    list(APPEND world_mesh_validation_inputs
        "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation/VkLayer_khronos_validation.json"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation/VkLayer_khronos_validation.dll"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation/vk_layer_settings.txt")
endif()
add_custom_target(octaryn_stage_client_world_mesh_probe
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${world_mesh_fixture}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets" "${world_mesh_fixture}/Assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data" "${world_mesh_fixture}/Data"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders" "${world_mesh_fixture}/Client/Shaders"
    ${world_mesh_runtime_commands}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/SamplingDiagnostic.slang"
        "${world_mesh_fixture}/Client/Shaders/Voxel/SamplingDiagnostic.slang"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/SamplingReference.slang"
        "${world_mesh_fixture}/Client/Shaders/Voxel/SamplingReference.slang"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/AtlasFiltering.slang"
        "${world_mesh_fixture}/Client/Shaders/Voxel/AtlasFiltering.slang"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/RelativePrecision.slang"
        "${world_mesh_fixture}/Client/Shaders/Voxel/RelativePrecision.slang"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldMeshProbe/PatchCoordinates.slang"
        "${world_mesh_fixture}/Client/Shaders/Voxel/PatchCoordinates.slang"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_client_world_mesh_probe>" "${world_mesh_fixture}/"
    DEPENDS octaryn_client_world_mesh_probe octaryn_client_shaders ${world_mesh_validation_inputs}
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
add_custom_target(octaryn_validate_client_world_mesh
    COMMAND "${CMAKE_COMMAND}" -E env ${world_mesh_validation_environment} --
        "${world_mesh_fixture}/$<TARGET_FILE_NAME:octaryn_client_world_mesh_probe>"
    DEPENDS octaryn_stage_client_world_mesh_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
# Explicit hardware qualification: unlike the normal fallback renderer, this
# target requires real bindless multi-draw and fails if it cannot exercise it.
add_custom_target(octaryn_validate_client_world_batch
    COMMAND "${CMAKE_COMMAND}" -E env ${world_mesh_validation_environment} --
        "${world_mesh_fixture}/$<TARGET_FILE_NAME:octaryn_client_world_mesh_probe>" --batch-only
    DEPENDS octaryn_stage_client_world_mesh_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
unset(world_mesh_fixture)
unset(world_mesh_runtime_commands)
unset(world_mesh_validation_environment)
unset(world_mesh_validation_inputs)
