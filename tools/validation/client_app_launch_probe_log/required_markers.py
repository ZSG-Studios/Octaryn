REQUIRED_LINES = (
    "window_show=0",
    "gpu_device_create=0",
    "gpu_window_claim=0",
    "game_module_descriptor=loaded",
    "block_atlas_manifest=loaded",
    "block_animation_manifest=loaded",
    "block_catalog=loaded",
    "initialize=0",
    "world_blocks_snapshot=0",
    "gpu_render_path=SDL_GPU",
    "material_atlas_tiles_drawn=2",
    "shutdown=0",
)

REQUIRED_PREFIXES = (
    "live_chunk_streaming active=1 source=server_process",
    "live_client_tick_input frame=1 dt=0.016667 flags=31 controller=1",
    "live_input_frame frame=1 active=1 move=(1.000,1.000,1.000)",
    "live_camera_frame frame=1 active=1 mode=live_runtime",
    "live_movement_frame frame=1 active=1",
    "live_interaction_frame frame=1 primary=1 secondary=1 command_enqueue_hook=active commands_enqueued=",
    "live_presentation_frame frame=1",
    "live_chunk_mesh_drain frame=1 active=1",
    "live_chunk_mesh_retained frame=1 active=1",
    "live_chunk_mesh_upload frame=1 active=1 target=sdl_gpu",
    "live_native_schedule_runtime frame=1 active=1 source=server_seed_memory",
    "block_atlas_texture=loaded",
    "block_atlas_normal_texture=loaded",
    "block_atlas_specular_texture=loaded",
    "block_atlas_animation_texture=loaded",
    "live_shader_pipeline active=1 sky=1 world=1 opaque_sprite=1 present=1 composite=1 ui=1 block_highlight=texture",
    "live_sky_pass active=1 source=server_world_time",
    "live_sky_pixel active=1 source=gpu_readback",
    "live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline active=1",
    "live_chunk_view_intent source=process_file",
)


def validate_required_markers(log_file, lines, errors):
    for expected in REQUIRED_LINES:
        if expected not in lines:
            errors.append(f"{log_file}: missing expected line {expected!r}, actual {lines}")

    for expected in REQUIRED_PREFIXES:
        if not any(line.startswith(expected) for line in lines):
            errors.append(f"{log_file}: missing expected line prefix {expected!r}, actual {lines}")

    tick_count = sum(1 for line in lines if line == "tick=0")
    if tick_count < 2:
        errors.append(f"{log_file}: expected at least two successful ticks, actual {lines}")
