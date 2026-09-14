def validate_streaming_and_rendering(log_file, lines, errors):
    forbidden = [line for line in lines if "SDL_GPU" in line or "sdl_gpu" in line or ".glsl" in line or "gpu_render_path=Slang_RHI" in line]
    if forbidden:
        errors.append(f"{log_file}: active renderer bootstrap must not log SDL GPU, GLSL, or completed-GPU-path markers: {forbidden}")
