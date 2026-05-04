def validate_log_order(log_file, lines, errors):
    try:
        module_descriptor_index = lines.index("game_module_descriptor=loaded")
        atlas_index = lines.index("block_atlas_manifest=loaded")
        animation_manifest_index = lines.index("block_animation_manifest=loaded")
        catalog_index = lines.index("block_catalog=loaded")
        atlas_texture_index = lines.index("block_atlas_texture=loaded")
        atlas_normal_texture_index = lines.index("block_atlas_normal_texture=loaded")
        atlas_specular_texture_index = lines.index("block_atlas_specular_texture=loaded")
        atlas_animation_texture_index = lines.index("block_atlas_animation_texture=loaded")
        initialize_index = lines.index("initialize=0")
        chunk_streaming_index = next(index for index, line in enumerate(lines) if line.startswith("live_chunk_streaming active=1 source=server_process"))
        snapshot_index = lines.index("world_blocks_snapshot=0")
        tick_input_index = next(index for index, line in enumerate(lines) if line.startswith("live_client_tick_input frame=1 dt=0.016667 flags=31 controller=1"))
        player_input_intent_index = next(index for index, line in enumerate(lines) if line.startswith("live_player_input_intent source=process_file "))
        input_index = next(index for index, line in enumerate(lines) if line.startswith("live_input_frame frame=1"))
        camera_index = next(index for index, line in enumerate(lines) if line.startswith("live_camera_frame frame=1 active=1 mode=live_runtime"))
        movement_index = next(index for index, line in enumerate(lines) if line.startswith("live_movement_frame frame=1 active=1"))
        interaction_index = next(index for index, line in enumerate(lines) if line.startswith("live_interaction_frame frame=1 primary=1 secondary=1 command_enqueue_hook=active commands_enqueued="))
        presentation_frame_index = next(index for index, line in enumerate(lines) if line.startswith("live_presentation_frame frame=1"))
        mesh_drain_index = next(index for index, line in enumerate(lines) if line.startswith("live_chunk_mesh_drain frame=1 active=1"))
        mesh_retained_index = next(index for index, line in enumerate(lines) if line.startswith("live_chunk_mesh_retained frame=1 active=1"))
        mesh_upload_index = next(index for index, line in enumerate(lines) if line.startswith("live_chunk_mesh_upload frame=1 active=1 target=sdl_gpu"))
        gpu_path_index = lines.index("gpu_render_path=SDL_GPU")
        material_index = lines.index("material_atlas_tiles_drawn=2")
        shutdown_index = lines.index("shutdown=0")
    except (StopIteration, ValueError):
        return

    expected_order = (
        module_descriptor_index,
        atlas_index,
        animation_manifest_index,
        catalog_index,
        atlas_texture_index,
        atlas_normal_texture_index,
        atlas_specular_texture_index,
        atlas_animation_texture_index,
        initialize_index,
        chunk_streaming_index,
        snapshot_index,
        player_input_intent_index,
        tick_input_index,
        mesh_drain_index,
        mesh_retained_index,
        mesh_upload_index,
        input_index,
        camera_index,
        movement_index,
        interaction_index,
        presentation_frame_index,
        gpu_path_index,
        material_index,
        shutdown_index,
    )
    if list(expected_order) != sorted(expected_order):
        errors.append(
            f"{log_file}: expected bundled module descriptor before atlas manifest/animation manifest/catalog/material texture loads before initialize before live chunk/input/camera/presentation logs before mesh upload before material atlas draw before shutdown, actual {lines}"
        )
