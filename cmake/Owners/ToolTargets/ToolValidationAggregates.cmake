add_custom_target(octaryn_validate_static DEPENDS
    octaryn_validate_render_pipeline
    octaryn_validate_cmake_targets
    octaryn_validate_cmake_policy_separation
    octaryn_validate_cmake_dependency_aliases
    octaryn_validate_package_policy_sync
    octaryn_validate_project_references
    octaryn_validate_module_layout
    octaryn_validate_basegame_block_catalog
    octaryn_validate_basegame_worldgen_content
    octaryn_validate_native_abi_contracts
    octaryn_validate_remote_wire
    octaryn_validate_native_owner_boundaries)

add_custom_target(octaryn_validate_cpu DEPENDS
    octaryn_validate_module_manifest_packages
    octaryn_validate_module_manifest_files
    octaryn_validate_module_source_api
    octaryn_validate_module_manifest_probe
    octaryn_validate_bundle_module_payload
    octaryn_validate_client_server_app
    octaryn_validate_remote_loopback
    octaryn_validate_module_binary_sandbox
    octaryn_validate_dotnet_package_assets
    octaryn_validate_native_archive_format
    octaryn_validate_native_jobs_probe
    octaryn_validate_client_voxel_invariants_probe
    octaryn_validate_client_voxel_mesh_probe
    octaryn_validate_client_voxel_indirect_probe
    octaryn_validate_temporal_resolution
    octaryn_validate_client_frame_metrics
    octaryn_validate_client_settings
    octaryn_validate_client_inventory
    octaryn_validate_client_ui_metrics
    octaryn_validate_client_world_stream
    octaryn_validate_client_interaction
    octaryn_validate_client_session_io
    octaryn_validate_client_player_model
    octaryn_validate_client_draw_preparation
    octaryn_validate_client_action_audio
    octaryn_validate_client_world_items
    octaryn_validate_server_world_items
    octaryn_validate_server_fluid
    octaryn_validate_terrain_cache
    octaryn_validate_server_host_policy_native_probe
    octaryn_validate_dotnet_owners
    octaryn_validate_world_time_probe
    octaryn_validate_server_world_time_native_probe
    octaryn_validate_server_authority_tick_native_probe
    octaryn_validate_server_block_store_native_probe
    octaryn_validate_server_terrain_generation_native_probe
    octaryn_validate_server_player_simulation_native_probe
    octaryn_validate_server_world_persistence_native_probe
    octaryn_validate_server_persistence_probe
    octaryn_validate_server_world_blocks_probe
    octaryn_validate_server_world_generation_probe
    octaryn_validate_basegame_player_probe
    octaryn_validate_basegame_interaction_probe
    octaryn_validate_owner_module_validation_probe
    octaryn_validate_hostfxr_bridge_exports
    octaryn_validate_owner_launch_probes)

add_custom_target(octaryn_validate_gpu DEPENDS
    octaryn_validate_client_shader_bundle
    octaryn_validate_client_render_backend_probe
    octaryn_validate_client_rhi_diagnostic
    octaryn_validate_client_rml_ui
    octaryn_validate_client_descriptor_allocator
    octaryn_validate_client_raster_culling
    octaryn_validate_client_player_rendering)
if(WIN32)
    add_dependencies(octaryn_validate_gpu octaryn_validate_client_fsr2_dx12 octaryn_validate_client_fsr2_vulkan)
elseif(APPLE)
    add_dependencies(octaryn_validate_gpu octaryn_validate_client_fsr2_metal)
else()
    add_dependencies(octaryn_validate_gpu octaryn_validate_client_fsr2_vulkan)
endif()

# Validation is explicit; ordinary builds do not launch diagnostic programs.
add_custom_target(octaryn_validate_all DEPENDS
    octaryn_validate_static octaryn_validate_cpu octaryn_validate_gpu)
