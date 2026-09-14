REQUIRED_EXACT_LINES = (
    "renderer_cutover_stage=slang_rhi_bootstrap",
    "slang_rhi_device=created runtime_available=1",
    "slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1",
    "slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1",
    "client_app_frame_loop frames=3 requested_frames=3 retained_voxel_session=1",
    "client_render_backend active=1 backend=slang_rhi shader_language=slang legacy_glsl=0 legacy_sdl_renderer=0 device_created=1",
    "client_voxel_renderer_rebuild active=0 raster_only=1 packed_quads=1 gpu_driven=planned",
    "shutdown=0",
)

REQUIRED_PREFIX_LINES = (
    "slang_rhi_swapchain=",
    "client_voxel_raster_runtime status=gpu_voxel_raster_frame_validated",
    "client_voxel_world_frame_loop requested_frames=",
)

REQUIRED_WORLD_FRAME_LOOP_TOKENS = (
    "stream_source=live_sidecar",
    "live_columns=4225",
    "radius32_stream_available=1",
    "indirect_draw=1",
    "readback=1",
    "retained_gpu_bytes=",
    "upload_staging_bytes=",
)


def validate_required_markers(log_file, lines, errors):
    for expected in REQUIRED_EXACT_LINES:
        if expected not in lines:
            errors.append(f"{log_file}: missing expected Slang RHI-backed bootstrap line {expected!r}, actual {lines}")

    for prefix in REQUIRED_PREFIX_LINES:
        if not any(line.startswith(prefix) for line in lines):
            errors.append(f"{log_file}: missing expected Slang RHI-backed bootstrap prefix {prefix!r}, actual {lines}")

    frame_loop_line = next((line for line in lines if line.startswith("client_voxel_world_frame_loop requested_frames=")), "")
    missing_tokens = [token for token in REQUIRED_WORLD_FRAME_LOOP_TOKENS if token not in frame_loop_line]
    if missing_tokens:
        errors.append(f"{log_file}: missing expected voxel frame-loop tokens {missing_tokens!r}, actual {frame_loop_line!r}")
