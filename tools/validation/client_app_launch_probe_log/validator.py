from .parsing import read_log_lines
from .required_markers import (
    REQUIRED_EXACT_LINES,
    REQUIRED_PREFIX_LINES,
    REQUIRED_WORLD_FRAME_LOOP_TOKENS,
)


def validate(log_file):
    if not log_file.exists():
        return [f"{log_file}: missing client app launch probe log"]

    lines = read_log_lines(log_file)
    if not lines or not lines[0].startswith("crash_marker=/tmp/octaryn-crash-"):
        return [f"{log_file}: missing crash diagnostics marker line, actual {lines}"]

    errors = []
    for line in REQUIRED_EXACT_LINES:
        if line not in lines:
            errors.append(f"{log_file}: missing expected Slang RHI-backed bootstrap line {line!r}, actual {lines}")
    for prefix in REQUIRED_PREFIX_LINES:
        if not any(line.startswith(prefix) for line in lines):
            errors.append(f"{log_file}: missing expected Slang RHI-backed bootstrap prefix {prefix!r}, actual {lines}")
    frame_loop_line = next((line for line in lines if line.startswith("client_voxel_world_frame_loop requested_frames=")), "")
    missing_tokens = [token for token in REQUIRED_WORLD_FRAME_LOOP_TOKENS if token not in frame_loop_line]
    if missing_tokens:
        errors.append(f"{log_file}: missing expected voxel frame-loop tokens {missing_tokens!r}, actual {frame_loop_line!r}")
    forbidden = [line for line in lines if "SDL_GPU" in line or "sdl_gpu" in line or ".glsl" in line or "gpu_render_path=Slang_RHI" in line]
    if forbidden:
        errors.append(f"{log_file}: legacy or overclaimed renderer marker survived active bootstrap: {forbidden}")
    return errors
