octaryn_add_native_executable(octaryn_temporal_resolution_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/TemporalResolutionValidation.cpp"
    PUBLIC_INCLUDE_DIRS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend")
add_custom_target(octaryn_validate_temporal_resolution
    COMMAND "$<TARGET_FILE:octaryn_temporal_resolution_probe>"
    DEPENDS octaryn_temporal_resolution_probe VERBATIM)
octaryn_add_native_executable(octaryn_client_frame_metrics_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/FrameMetricsProbe.cpp"
    PRIVATE_LINKS octaryn_client_frame_metrics)
add_custom_target(octaryn_validate_client_frame_metrics
    COMMAND "$<TARGET_FILE:octaryn_client_frame_metrics_probe>"
    DEPENDS octaryn_client_frame_metrics_probe VERBATIM)

octaryn_add_native_executable(octaryn_client_settings_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientSettingsProbe/main.cpp"
    PRIVATE_LINKS octaryn_client_runtime_settings octaryn_client_app_settings octaryn::deps::sdl3 octaryn::deps::glaze)
set_target_properties(octaryn_client_settings_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(octaryn_validate_client_settings
    COMMAND "$<TARGET_FILE:octaryn_client_settings_probe>" "${OCTARYN_WORKSPACE_ROOT_DIR}/logs/client/settings-validation"
    DEPENDS octaryn_client_settings_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)

octaryn_add_native_executable(octaryn_client_inventory_probe tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/InventoryValidation.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/Inventory.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi/InventoryPersistence.cpp"
    PUBLIC_INCLUDE_DIRS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi"
    PRIVATE_LINKS octaryn::deps::glaze)
set_target_properties(octaryn_client_inventory_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(octaryn_validate_client_inventory
    COMMAND "$<TARGET_FILE:octaryn_client_inventory_probe>" "${OCTARYN_WORKSPACE_ROOT_DIR}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/logs/client/inventory-validation/cmake"
    DEPENDS octaryn_client_inventory_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)

octaryn_add_native_executable(octaryn_client_descriptor_allocator_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientDescriptorAllocatorProbe/main.cpp"
    PRIVATE_LINKS octaryn::deps::slang_rhi)
target_include_directories(octaryn_client_descriptor_allocator_probe SYSTEM PRIVATE
    "${OCTARYN_SLANG_RHI_SOURCE_ROOT}/src"
    "${OCTARYN_SLANG_RHI_BUILD_ROOT}/_deps/vulkan_headers-src/include")
target_compile_definitions(octaryn_client_descriptor_allocator_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN SLANG_RHI_DEBUG=0)
set_target_properties(octaryn_client_descriptor_allocator_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(octaryn_validate_client_descriptor_allocator
    COMMAND "$<TARGET_FILE:octaryn_client_descriptor_allocator_probe>"
    DEPENDS octaryn_client_descriptor_allocator_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)

octaryn_add_native_executable(octaryn_client_ui_metrics_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientUiMetricsProbe/main.cpp"
    PUBLIC_INCLUDE_DIRS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Ui/GameUi"
    PRIVATE_LINKS octaryn::deps::rmlui)
set_target_properties(octaryn_client_ui_metrics_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(octaryn_validate_client_ui_metrics
    COMMAND "$<TARGET_FILE:octaryn_client_ui_metrics_probe>" "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets/Ui"
    DEPENDS octaryn_client_ui_metrics_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)

octaryn_add_native_executable(octaryn_server_fluid_probe tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerFluidProbe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerFluidProbe/Apply.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerFluidProbe/Scheduler.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerFluidProbe/FluidAbi.cpp"
    PRIVATE_LINKS octaryn_server_block_store_internal)
set_target_properties(octaryn_server_fluid_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(octaryn_validate_server_fluid
    COMMAND "$<TARGET_FILE:octaryn_server_fluid_probe>"
    DEPENDS octaryn_server_fluid_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

octaryn_add_native_executable(
    octaryn_client_world_stream_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientWorldStreamProbe/ClientWorldStreamProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Source/Gameplay/Terrain"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldStream"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Generation"
    PRIVATE_LINKS
        octaryn_client_world_stream
        octaryn_server_terrain_generation)
set_target_properties(octaryn_client_world_stream_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
set(client_world_stream_probe_launcher)
if(WIN32)
    set(client_world_stream_probe_launcher
        "${CMAKE_COMMAND}" -E env
        --modify "PATH=path_list_prepend:$<TARGET_FILE_DIR:octaryn_native_jobs>"
        --modify "PATH=path_list_prepend:$<TARGET_FILE_DIR:octaryn_server_terrain_generation>"
        --)
endif()
add_custom_target(octaryn_validate_client_world_stream
    COMMAND ${client_world_stream_probe_launcher} "$<TARGET_FILE:octaryn_client_world_stream_probe>"
    DEPENDS octaryn_client_world_stream_probe octaryn_server_terrain_generation octaryn_native_jobs
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
octaryn_add_native_executable(octaryn_client_interaction_probe tools
    SOURCES "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientInteractionProbe/ClientInteractionProbe.cpp"
    PRIVATE_LINKS octaryn_client_block_interaction octaryn_client_world_stream)
set_target_properties(octaryn_client_interaction_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
if(CMAKE_CROSSCOMPILING)
    add_custom_target(octaryn_validate_client_interaction
        COMMAND "${CMAKE_COMMAND}" -E echo "Cannot execute the interaction probe while cross-compiling."
        COMMAND "${CMAKE_COMMAND}" -E false VERBATIM)
else()
    add_custom_target(octaryn_validate_client_interaction
        COMMAND ${client_world_stream_probe_launcher} "$<TARGET_FILE:octaryn_client_interaction_probe>"
            "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json"
        DEPENDS octaryn_client_interaction_probe octaryn_native_jobs octaryn_server_terrain_generation
        WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}" VERBATIM)
endif()
unset(client_world_stream_probe_launcher)

octaryn_add_native_executable(
    octaryn_native_jobs_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/NativeJobsProbe/NativeJobsProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/NativeJobs"
    PRIVATE_LINKS
        octaryn_native_jobs
        octaryn::deps::taskflow)

octaryn_add_native_executable(
    octaryn_client_render_backend_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientRenderBackendProbe/ClientRenderBackendProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend"
    PRIVATE_LINKS
        octaryn_client_render_backend)

octaryn_add_native_executable(
    octaryn_client_voxel_invariants_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientVoxelInvariantsProbe/ClientVoxelInvariantsProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Settings/RenderDistance"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelWorld"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/ChunkView"
    PRIVATE_LINKS
        octaryn_client_render_distance
        octaryn_client_chunk_view
        octaryn_client_voxel_world)

octaryn_add_native_executable(
    octaryn_client_voxel_mesh_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientVoxelMeshProbe/ClientVoxelMeshProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelWorld"
    PRIVATE_LINKS
        octaryn_client_voxel_world)
octaryn_add_native_executable(
    octaryn_client_voxel_indirect_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientVoxelIndirectProbe/ClientVoxelIndirectProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/VoxelWorld"
    PRIVATE_LINKS
        octaryn_client_voxel_world)
octaryn_add_native_executable(
    octaryn_server_host_policy_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerHostPolicyProbe/ServerHostPolicyProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Host"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
    PRIVATE_LINKS
        octaryn_server_host)

octaryn_add_native_executable(
    octaryn_server_world_time_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldTimeProbe/ServerWorldTimeProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Time"
    PRIVATE_LINKS
        octaryn_server_world_time_internal)

octaryn_add_native_executable(
    octaryn_server_authority_tick_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerAuthorityTickProbe/ServerAuthorityTickProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Tick"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/NativeJobs"
    PRIVATE_LINKS
        octaryn_server_authority_tick
        octaryn_native_jobs)

octaryn_add_native_executable(
    octaryn_server_block_store_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/ServerBlockStoreProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/SnapshotCount.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/Backpressure.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/ServerBlockStoreCommandQueueProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/ServerBlockStoreCommandValidationProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/ServerBlockStoreChunkRequestProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/ServerBlockStoreChunkStreamProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/ServerBlockStoreProcessSnapshotProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerBlockStoreProbe/ServerBlockStoreProcessTickProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Chunks/Streaming"
    PRIVATE_LINKS
        octaryn_server_block_store_internal)

octaryn_add_native_executable(
    octaryn_server_player_simulation_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerPlayerSimulationProbe/ServerPlayerSimulationProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerPlayerSimulationProbe/ServerPlayerSimulationInputIntentProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerPlayerSimulationProbe/ServerPlayerSimulationMovementProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerPlayerSimulationProbe/ServerPlayerSimulationSaveProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerPlayerSimulationProbe/ServerPlayerSimulationStepProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Simulation/Players"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/HostAbi"
    PRIVATE_LINKS
        octaryn_server_player_simulation
        octaryn_server_block_store_internal)

octaryn_add_native_executable(
    octaryn_server_terrain_generation_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerTerrainGenerationProbe/ServerTerrainGenerationProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerTerrainGenerationProbe/ColumnCache.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerTerrainGenerationProbe/Features.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Blocks/Store"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/World/Generation"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Source/Gameplay/Terrain"
    PRIVATE_LINKS
        octaryn_server_terrain_generation
        octaryn_native_threads
        octaryn_server_block_store_internal)

octaryn_add_native_executable(
    octaryn_server_world_persistence_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/ChunkOverrideDirectoryProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/GzipPersistenceProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/PathPolicyProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/PlayerDirectoryProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/SaveExportBundleProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/ServerWorldPersistenceProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/WorldBlockExportColumnPlanProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/WorldBlockPersistencePolicyProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/WorldMetadataBuildProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/WorldGenerationIdentityProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe/WorldSaveImportProbe.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ServerWorldPersistenceProbe"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Persistence/WorldBlocks"
    PRIVATE_LINKS
        octaryn_server_world_persistence)


octaryn_add_native_executable(
    octaryn_client_player_model_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientPlayerModelProbe/ClientPlayerModelProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientPlayerModelProbe/PlayerPresentationProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientPlayerModelProbe/PoseHistoryProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientPlayerModelProbe/StreamResidencyProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientPlayerModelProbe/ColumnBlocksProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientPlayerModelProbe/ActionFeedbackProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/PlayerPresentation.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/LocalSession"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/WorldPresentation/WorldStream"
    PRIVATE_LINKS octaryn_client_render_backend octaryn::deps::fastgltf
        octaryn_client_action_audio octaryn_client_block_interaction
        octaryn_client_player_control_input octaryn_client_runtime_controls octaryn::deps::sdl3)
add_dependencies(octaryn_tools octaryn_client_player_model_probe)
add_custom_target(octaryn_validate_client_player_model
    COMMAND "$<TARGET_FILE:octaryn_client_player_model_probe>"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets/Player/octaryn_player_v1.gltf"
        "${OCTARYN_BUILD_PRESET_ROOT}/client/validation/player-model"
    DEPENDS octaryn_client_player_model_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

octaryn_add_native_executable(
    octaryn_client_draw_preparation_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientDrawPreparationProbe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientDrawPreparationProbe/HaloInvalidationProbe.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientDrawPreparationProbe/CullingProbe.cpp"
    PRIVATE_LINKS octaryn_client_render_backend octaryn_client_lighting_settings octaryn_client_camera octaryn_client_world_stream
        octaryn::deps::slang_rhi octaryn::deps::sdl3)
add_dependencies(octaryn_tools octaryn_client_draw_preparation_probe)
add_custom_target(octaryn_validate_client_draw_preparation
    COMMAND "$<TARGET_FILE:octaryn_client_draw_preparation_probe>"
    DEPENDS octaryn_client_draw_preparation_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

octaryn_add_native_executable(
    octaryn_client_raster_culling_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientRasterCullingProbe/main.cpp"
    PRIVATE_LINKS octaryn_client_render_backend octaryn_client_lighting_settings
        octaryn::deps::slang_rhi octaryn::deps::sdl3)
set_target_properties(octaryn_client_raster_culling_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
set(client_raster_fixture "${OCTARYN_BUILD_PRESET_ROOT}/tools/validation/raster-culling")
set(client_raster_runtime_commands)
if(WIN32)
    foreach(runtime_file IN LISTS OCTARYN_CLIENT_SLANG_RUNTIME_FILES)
        list(APPEND client_raster_runtime_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_file}" "${client_raster_fixture}/")
    endforeach()
endif()
add_custom_target(octaryn_validate_client_raster_culling
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${client_raster_fixture}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets" "${client_raster_fixture}/Assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data" "${client_raster_fixture}/Data"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders" "${client_raster_fixture}/Client/Shaders"
    ${client_raster_runtime_commands}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_client_raster_culling_probe>" "${client_raster_fixture}/"
    COMMAND "${client_raster_fixture}/$<TARGET_FILE_NAME:octaryn_client_raster_culling_probe>"
    DEPENDS octaryn_client_raster_culling_probe octaryn_client_shaders
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
unset(client_raster_runtime_commands)
unset(client_raster_fixture)

octaryn_add_native_executable(
    octaryn_client_player_rendering_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientPlayerRenderingProbe/main.cpp"
    PRIVATE_LINKS octaryn_client_render_backend octaryn_client_asset_paths
        octaryn::deps::fastgltf octaryn::deps::slang_rhi)
set_target_properties(octaryn_client_player_rendering_probe PROPERTIES EXCLUDE_FROM_ALL TRUE)
set(client_player_fixture "${OCTARYN_BUILD_PRESET_ROOT}/tools/validation/player-rendering")
set(client_player_runtime_commands)
set(client_player_validation_environment)
set(client_player_validation_inputs)
if(WIN32)
    foreach(runtime_file IN LISTS OCTARYN_CLIENT_SLANG_RUNTIME_FILES)
        list(APPEND client_player_runtime_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_file}" "${client_player_fixture}/")
    endforeach()
    list(APPEND client_player_validation_environment
        "VK_LAYER_PATH=${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation"
        "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"
        "VK_LAYER_SETTINGS_PATH=${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation")
    list(APPEND client_player_validation_inputs
        "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation/VkLayer_khronos_validation.json"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation/VkLayer_khronos_validation.dll"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/vulkan-validation/vk_layer_settings.txt")
endif()
add_custom_target(octaryn_validate_client_player_rendering
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${client_player_fixture}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Assets/Player" "${client_player_fixture}/Client/Assets/Player"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders" "${client_player_fixture}/Client/Shaders"
    ${client_player_runtime_commands}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:octaryn_client_player_rendering_probe>" "${client_player_fixture}/"
    COMMAND "${CMAKE_COMMAND}" -E env ${client_player_validation_environment} --
        "${client_player_fixture}/$<TARGET_FILE_NAME:octaryn_client_player_rendering_probe>"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/logs/client"
    DEPENDS octaryn_client_player_rendering_probe octaryn_client_shaders ${client_player_validation_inputs}
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
unset(client_player_runtime_commands)
unset(client_player_validation_environment)
unset(client_player_validation_inputs)
unset(client_player_fixture)

octaryn_add_native_executable(
    octaryn_client_action_audio_probe
    tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientActionAudioProbe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/ClientActionAudioProbe/Definitions.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/ActionSounds.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld"
    PRIVATE_LINKS octaryn_client_action_audio octaryn::deps::glaze)
add_dependencies(octaryn_tools octaryn_client_action_audio_probe)
add_custom_target(octaryn_validate_client_action_audio
    COMMAND "$<TARGET_FILE:octaryn_client_action_audio_probe>" --cpu
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets/Audio/action-sounds.json"
        "${OCTARYN_BUILD_PRESET_ROOT}/client/validation/action-audio"
    DEPENDS octaryn_client_action_audio_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
add_custom_target(octaryn_validate_client_action_audio_loopback
    COMMAND "$<TARGET_FILE:octaryn_client_action_audio_probe>" --loopback
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets/Audio/action-sounds.json"
        "${OCTARYN_BUILD_PRESET_ROOT}/client/validation/action-audio-loopback"
    DEPENDS octaryn_client_action_audio_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

include("${CMAKE_CURRENT_LIST_DIR}/ToolMovementTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ToolSessionIoTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ToolWorldMeshProbe.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ToolWorldItemsTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ToolFsr2Probe.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ToolTerrainCacheProbe.cmake")
