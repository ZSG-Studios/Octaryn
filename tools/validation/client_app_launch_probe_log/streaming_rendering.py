from .parsing import parse_named_float, parse_named_int, parse_positive_count


def validate_streaming_and_rendering(log_file, lines, errors):
    stream_columns = parse_positive_count(lines, "server_chunk_stream_columns=")
    if not stream_columns or max(stream_columns) < 2:
        errors.append(f"{log_file}: expected multi-column server chunk stream columns, actual {lines}")

    stream_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_streaming active=1 source=server_process")
    ]
    if (
        not stream_lines
        or parse_named_int(stream_lines[0], "radius") is None
        or parse_named_int(stream_lines[0], "radius") < 1
    ):
        errors.append(f"{log_file}: expected server-process chunk streaming radius above zero, actual {stream_lines[0] if stream_lines else lines!r}")

    _validate_atlas_assets(log_file, lines, errors)
    _validate_mesh_pipeline(log_file, lines, errors)
    _validate_sky_and_world_draw(log_file, lines, errors)


def _validate_atlas_assets(log_file, lines, errors):
    atlas_layers = parse_positive_count(lines, "block_atlas_layers=")
    if not atlas_layers or max(atlas_layers) <= 0:
        errors.append(f"{log_file}: expected block atlas layers, actual {lines}")

    atlas_tile_size = parse_positive_count(lines, "block_atlas_tile_size=")
    if not atlas_tile_size or max(atlas_tile_size) <= 0:
        errors.append(f"{log_file}: expected positive block atlas tile size, actual {lines}")

    animation_tile_size = parse_positive_count(lines, "block_animation_tile_size=")
    if not animation_tile_size or max(animation_tile_size) <= 0:
        errors.append(f"{log_file}: expected positive block animation tile size, actual {lines}")

    animation_frames = parse_positive_count(lines, "block_animation_frames=")
    if not animation_frames:
        errors.append(f"{log_file}: expected block animation frames marker, actual {lines}")

    animation_count = parse_positive_count(lines, "block_animation_count=")
    if not animation_count:
        errors.append(f"{log_file}: expected block animation count marker, actual {lines}")

    catalog_entries = parse_positive_count(lines, "block_catalog_entries=")
    if not catalog_entries or max(catalog_entries) <= 0:
        errors.append(f"{log_file}: expected block catalog entries, actual {lines}")

    material_tiles = parse_positive_count(lines, "material_atlas_tiles_drawn=")
    if not material_tiles or max(material_tiles) != 2:
        errors.append(f"{log_file}: expected normal/specular material atlas tiles drawn, actual {lines}")

    for prefix in (
        "block_atlas_texture=loaded",
        "block_atlas_normal_texture=loaded",
        "block_atlas_specular_texture=loaded",
    ):
        atlas_lines = [line for line in lines if line.startswith(prefix)]
        mip_levels = parse_named_int(atlas_lines[0], "mip_levels") if atlas_lines else None
        if mip_levels is None or mip_levels <= 1:
            errors.append(f"{log_file}: expected mipmapped material atlas upload for {prefix}, actual {atlas_lines[0] if atlas_lines else lines!r}")


def _validate_mesh_pipeline(log_file, lines, errors):
    if not any(line.startswith("gpu_swapchain_acquired width=") for line in lines):
        errors.append(f"{log_file}: expected SDL GPU swapchain acquisition, actual {lines}")

    mesh_drain_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_mesh_drain frame=1 active=1")
    ]
    if (
        not mesh_drain_lines
        or "chunks=" not in mesh_drain_lines[0]
        or "opaque_faces=" not in mesh_drain_lines[0]
        or "fluid_blocks=" not in mesh_drain_lines[0]
    ):
        errors.append(f"{log_file}: expected managed chunk mesh drain counters, actual {lines}")
    else:
        mesh_drain_opaque = parse_positive_count(
            [mesh_drain_lines[0].split(" opaque_faces=", 1)[1].split(" ", 1)[0]],
            "",
        )
        if not mesh_drain_opaque or mesh_drain_opaque[0] <= 1:
            errors.append(f"{log_file}: expected opaque chunk mesh faces from streamed blocks, actual {mesh_drain_lines[0]!r}")

    mesh_retained_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_mesh_retained frame=1 active=1")
    ]
    if (
        not mesh_retained_lines
        or "visible_chunks=" not in mesh_retained_lines[0]
        or "opaque_faces=" not in mesh_retained_lines[0]
    ):
        errors.append(f"{log_file}: expected retained chunk mesh counters, actual {lines}")

    mesh_upload_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_mesh_upload frame=1 active=1 target=sdl_gpu")
    ]
    if (
        not mesh_upload_lines
        or "chunks=" not in mesh_upload_lines[0]
        or "opaque_bytes=" not in mesh_upload_lines[0]
        or "target=sdl_gpu_direct_indirect" not in mesh_upload_lines[0]
    ):
        errors.append(f"{log_file}: expected retained direct-indirect SDL GPU chunk mesh upload counters, actual {lines}")
    else:
        mesh_upload_chunks = parse_named_int(mesh_upload_lines[0], "chunks")
        if mesh_upload_chunks is None or mesh_upload_chunks < 1:
            errors.append(f"{log_file}: expected at least one streamed chunk mesh upload, actual {mesh_upload_lines[0]!r}")
        mesh_upload_opaque_bytes = parse_positive_count(
            [mesh_upload_lines[0].split(" opaque_bytes=", 1)[1].split(" ", 1)[0]],
            "",
        )
        if not mesh_upload_opaque_bytes or mesh_upload_opaque_bytes[0] <= 8:
            errors.append(f"{log_file}: expected packed chunk mesh bytes uploaded to SDL GPU, actual {mesh_upload_lines[0]!r}")

    schedule_lines = [
        line
        for line in lines
        if line.startswith("live_native_schedule_runtime frame=1 active=1 source=server_seed_memory")
    ]
    if (
        not schedule_lines
        or parse_named_int(schedule_lines[0], "worker_jobs") is None
        or parse_named_int(schedule_lines[0], "worker_jobs") < 1
        or parse_named_int(schedule_lines[0], "main_thread_jobs") is None
        or parse_named_int(schedule_lines[0], "main_thread_jobs") < 1
        or parse_named_int(schedule_lines[0], "chunks") is None
        or parse_named_int(schedule_lines[0], "chunks") < 1
    ):
        errors.append(f"{log_file}: expected native scheduled runtime to route mesh build and GPU upload under launch load, actual {schedule_lines[0] if schedule_lines else lines!r}")

    batch_lines = [
        line
        for line in lines
        if line.startswith("live_server_stream_mesh_batch ")
        and " active=1 source=server_seed_memory" in line
    ]
    first_batch = batch_lines[0] if batch_lines else None
    batch_processed = parse_named_int(first_batch, "processed") if first_batch else None
    batch_build_ms = parse_named_float(first_batch, "build_ms") if first_batch else None
    batch_upload_ms = parse_named_float(first_batch, "upload_ms") if first_batch else None
    if (
        not batch_lines
        or batch_processed is None
        or batch_processed < 1
        or batch_processed > 128
        or batch_build_ms is None
        or batch_upload_ms is None
    ):
        errors.append(f"{log_file}: expected bounded server-stream mesh batch timing counters, actual {first_batch if first_batch else lines!r}")

    invalid_batches = []
    for line in batch_lines:
        processed = parse_named_int(line, "processed")
        if (
            processed is None
            or processed < 1
            or processed > 128
            or parse_named_float(line, "build_ms") is None
            or parse_named_float(line, "upload_ms") is None
        ):
            invalid_batches.append(line)
    if invalid_batches:
        errors.append(f"{log_file}: expected every server-stream mesh batch to stay bounded and timed, actual {invalid_batches!r}")
    if len(batch_lines) < 2:
        errors.append(f"{log_file}: expected multiple bounded server-stream mesh batches, actual {batch_lines!r}")


def _validate_sky_and_world_draw(log_file, lines, errors):
    sky_uniform_lines = [
        line
        for line in lines
        if line.startswith("live_sky_uniforms source=server_process")
    ]
    if not sky_uniform_lines or "day_fraction=" not in sky_uniform_lines[0] or "total_seconds=" not in sky_uniform_lines[0]:
        errors.append(f"{log_file}: expected server world-time sky uniforms, actual {lines}")

    sky_pixel_lines = [
        line
        for line in lines
        if line.startswith("live_sky_pixel active=1 source=gpu_readback")
    ]
    if (
        not sky_pixel_lines
        or "raw16=(" not in sky_pixel_lines[0]
        or "raw16=(0,0,0,0)" in sky_pixel_lines[0]
    ):
        errors.append(f"{log_file}: expected a non-clear sky pixel read back from the SDL GPU frame, actual {lines}")

    world_draw_lines = [
        line
        for line in lines
        if line.startswith("live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline active=1")
    ]
    if (
        not world_draw_lines
        or "chunks=" not in world_draw_lines[0]
        or "opaque_faces=" not in world_draw_lines[0]
        or "sprite_indices=" not in world_draw_lines[0]
        or "path=direct_indirect" not in world_draw_lines[0]
    ):
        errors.append(f"{log_file}: expected direct-indirect SDL GPU world mesh shader draw counters, actual {lines}")
    else:
        world_draw_faces = parse_positive_count(
            [world_draw_lines[0].split(" opaque_faces=", 1)[1].split(" ", 1)[0]],
            "",
        )
        if not world_draw_faces or world_draw_faces[0] <= 1:
            errors.append(f"{log_file}: expected opaque world mesh faces drawn by shader pipeline, actual {world_draw_lines[0]!r}")
