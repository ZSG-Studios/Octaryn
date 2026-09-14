def validate_log_order(log_file, lines, errors):
    try:
        crash = next(index for index, line in enumerate(lines) if line.startswith("crash_marker=/tmp/octaryn-crash-"))
        cutover = lines.index("renderer_cutover_stage=slang_rhi_bootstrap")
        device = lines.index("slang_rhi_device=created runtime_available=1")
        frame = lines.index("slang_rhi_frame_resources=offscreen_frame_resources_validated validated=1")
        lifecycle = lines.index("slang_rhi_frame_lifecycle=validated begun=1 encoded=1 ended=1 submitted=1")
        app_frame_loop = lines.index("client_app_frame_loop frames=3 requested_frames=3 retained_voxel_session=1")
        voxel_runtime = next(index for index, line in enumerate(lines) if line.startswith("client_voxel_raster_runtime "))
        voxel_frame_loop = next(index for index, line in enumerate(lines) if line.startswith("client_voxel_world_frame_loop "))
        swapchain = next(index for index, line in enumerate(lines) if line.startswith("slang_rhi_swapchain="))
        backend = next(index for index, line in enumerate(lines) if line.startswith("client_render_backend active=1 backend=slang_rhi"))
        rebuild = lines.index("client_voxel_renderer_rebuild active=0 raster_only=1 packed_quads=1 gpu_driven=planned")
        shutdown = lines.index("shutdown=0")
    except (StopIteration, ValueError):
        return
    order = (crash, cutover, device, frame, lifecycle, app_frame_loop, voxel_runtime, voxel_frame_loop, swapchain, backend, rebuild, shutdown)
    if list(order) != sorted(order):
        errors.append(f"{log_file}: expected Slang RHI bootstrap order crash->cutover->device->frame->lifecycle->app_frame_loop->voxel_runtime->voxel_frame_loop->swapchain->backend->rebuild->shutdown, actual {lines}")
