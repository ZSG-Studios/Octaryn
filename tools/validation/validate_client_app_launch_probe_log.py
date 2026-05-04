#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys


REQUIRED_LINES = (
    "window_show=0",
    "gpu_device_create=0",
    "gpu_window_claim=0",
    "basegame_module_descriptor=loaded",
    "basegame_atlas_manifest=loaded",
    "basegame_animation_manifest=loaded",
    "basegame_block_catalog=loaded",
    "basegame_atlas_texture=loaded",
    "basegame_atlas_normal_texture=loaded",
    "basegame_atlas_specular_texture=loaded",
    "basegame_atlas_animation_texture=loaded",
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
    "live_chunk_mesh_plan frame=1 active=1 source=managed_presentation_pipeline",
    "live_chunk_mesh_upload frame=1 active=1 target=sdl_gpu",
    "live_shader_pipeline active=1 sky=1 world=1 source=compiled_spirv",
    "live_sky_pass active=1 source=server_world_time",
    "live_sky_pixel active=1 source=sky_uniform_sample",
    "live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline active=1",
    "live_chunk_view_intent source=process_file",
    "live_block_interaction_intent source=process_file",
)
COMMAND_PATTERN = re.compile(
    r"^live_client_command_enqueue kind=1 request=(?P<request>\d+) target=0 "
    r"edit=(?P<edit>break|place) "
    r"block=\((?P<x>-?\d+),(?P<y>-?\d+),(?P<z>-?\d+),(?P<block>\d+)\) "
    r"flags=3$"
)


def parse_positive_count(lines, prefix):
    values = []
    for line in lines:
        if not line.startswith(prefix):
            continue
        try:
            values.append(int(line.removeprefix(prefix)))
        except ValueError:
            return []
    return values


def parse_named_float(line, name):
    match = re.search(rf"{name}=(-?\d+\.\d+)", line)
    if not match:
        return None
    return float(match.group(1))


def parse_named_int(line, name):
    match = re.search(rf"{name}=(-?\d+)", line)
    if not match:
        return None
    return int(match.group(1))


def close_to(value, expected, tolerance):
    return value is not None and abs(value - expected) <= tolerance


def parse_camera_tuple(line):
    match = re.search(
        r"camera=\((-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+)\)",
        line,
    )
    if not match:
        return None
    return tuple(float(value) for value in match.groups())


def validate(log_file):
    if not log_file.exists():
        return [f"{log_file}: missing client app launch probe log"]

    lines = [
        line.strip()
        for line in log_file.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    if not lines or not lines[0].startswith("crash_marker=/tmp/octaryn-crash-"):
        return [f"{log_file}: missing crash diagnostics marker line, actual {lines}"]

    errors = []
    for expected in REQUIRED_LINES:
        if expected not in lines:
            errors.append(f"{log_file}: missing expected line {expected!r}, actual {lines}")
    for expected in REQUIRED_PREFIXES:
        if not any(line.startswith(expected) for line in lines):
            errors.append(f"{log_file}: missing expected line prefix {expected!r}, actual {lines}")

    tick_count = sum(1 for line in lines if line == "tick=0")
    if tick_count < 2:
        errors.append(f"{log_file}: expected at least two successful ticks, actual {lines}")

    loaded_blocks = parse_positive_count(lines, "server_chunk_stream_loaded=")
    if not loaded_blocks or max(loaded_blocks) <= 1:
        errors.append(f"{log_file}: expected generated server chunk stream block load, actual {lines}")

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

    surface_blocks = parse_positive_count(lines, "server_chunk_stream_surface_blocks_applied=")
    if not surface_blocks or max(surface_blocks) <= 1:
        errors.append(f"{log_file}: expected generated server stream surface block snapshot, actual {lines}")

    atlas_layers = parse_positive_count(lines, "basegame_atlas_layers=")
    if not atlas_layers or max(atlas_layers) != 29:
        errors.append(f"{log_file}: expected 29 basegame atlas layers, actual {lines}")

    atlas_tile_size = parse_positive_count(lines, "basegame_atlas_tile_size=")
    if not atlas_tile_size or max(atlas_tile_size) != 32:
        errors.append(f"{log_file}: expected 32 px basegame atlas tiles, actual {lines}")

    animation_tile_size = parse_positive_count(lines, "basegame_animation_tile_size=")
    if not animation_tile_size or max(animation_tile_size) != 32:
        errors.append(f"{log_file}: expected 32 px basegame animation tiles, actual {lines}")

    animation_frames = parse_positive_count(lines, "basegame_animation_frames=")
    if not animation_frames or max(animation_frames) != 0:
        errors.append(f"{log_file}: expected empty basegame animation frames, actual {lines}")

    animation_count = parse_positive_count(lines, "basegame_animation_count=")
    if not animation_count or max(animation_count) != 0:
        errors.append(f"{log_file}: expected empty basegame animation count, actual {lines}")

    catalog_entries = parse_positive_count(lines, "basegame_block_catalog_entries=")
    if not catalog_entries or max(catalog_entries) != 39:
        errors.append(f"{log_file}: expected 39 basegame block catalog entries, actual {lines}")

    drained_updates = parse_positive_count(lines, "presentation_updates_drained=")
    if not drained_updates or sum(drained_updates) <= 1:
        errors.append(f"{log_file}: expected multiple presentation updates drained, actual {lines}")

    if not any(line.startswith("gpu_swapchain_acquired width=") for line in lines):
        errors.append(f"{log_file}: expected SDL GPU swapchain acquisition, actual {lines}")

    material_tiles = parse_positive_count(lines, "material_atlas_tiles_drawn=")
    if not material_tiles or max(material_tiles) != 2:
        errors.append(f"{log_file}: expected normal/specular material atlas tiles drawn, actual {lines}")

    presented_blocks = parse_positive_count(lines, "presented_block_count=")
    if not presented_blocks or max(presented_blocks) <= 1:
        errors.append(f"{log_file}: expected multiple presented blocks, actual {lines}")

    mesh_plan_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_mesh_plan frame=1 active=1 source=managed_presentation_pipeline")
    ]
    if (
        not mesh_plan_lines
        or "dirty_chunks=" not in mesh_plan_lines[0]
        or "opaque_faces=" not in mesh_plan_lines[0]
        or "fluid_blocks=" not in mesh_plan_lines[0]
    ):
        errors.append(f"{log_file}: expected managed chunk mesh plan counters, actual {lines}")
    else:
        mesh_plan_opaque = parse_positive_count(
            [mesh_plan_lines[0].split(" opaque_faces=", 1)[1].split(" ", 1)[0]],
            "",
        )
        if not mesh_plan_opaque or mesh_plan_opaque[0] <= 1:
            errors.append(f"{log_file}: expected opaque chunk mesh faces from streamed blocks, actual {mesh_plan_lines[0]!r}")

    mesh_upload_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_mesh_upload frame=1 active=1 target=sdl_gpu")
    ]
    if (
        not mesh_upload_lines
        or "chunks=" not in mesh_upload_lines[0]
        or "opaque_bytes=" not in mesh_upload_lines[0]
    ):
        errors.append(f"{log_file}: expected SDL GPU chunk mesh upload counters, actual {lines}")
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
        if line.startswith("live_sky_pixel active=1 source=sky_uniform_sample")
    ]
    if not sky_pixel_lines or "clear_match=0" not in sky_pixel_lines[0]:
        errors.append(f"{log_file}: expected a non-clear sky sample derived from the server-time shader uniforms, actual {lines}")

    world_draw_lines = [
        line
        for line in lines
        if line.startswith("live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline active=1")
    ]
    if not world_draw_lines or "chunks=" not in world_draw_lines[0] or "opaque_faces=" not in world_draw_lines[0]:
        errors.append(f"{log_file}: expected SDL GPU world mesh shader draw counters, actual {lines}")
    else:
        world_draw_faces = parse_positive_count(
            [world_draw_lines[0].split(" opaque_faces=", 1)[1].split(" ", 1)[0]],
            "",
        )
        if not world_draw_faces or world_draw_faces[0] <= 1:
            errors.append(f"{log_file}: expected opaque world mesh faces drawn by shader pipeline, actual {world_draw_lines[0]!r}")

    snapshot_origin_lines = [
        line
        for line in lines
        if line.startswith("snapshot_camera_origin x=")
    ]
    if not snapshot_origin_lines:
        errors.append(f"{log_file}: expected snapshot-relative camera origin, actual {lines}")
        snapshot_origin = None
    else:
        snapshot_origin = (
            parse_named_float(snapshot_origin_lines[0], "x"),
            parse_named_float(snapshot_origin_lines[0], "y"),
            parse_named_float(snapshot_origin_lines[0], "z"),
        )

    active_camera_lines = [
        line
        for line in lines
        if line.startswith("live_camera_frame frame=1 active=1 mode=live_runtime")
    ]
    if not active_camera_lines or "look=(-6.000,12.000)" not in active_camera_lines[0]:
        errors.append(f"{log_file}: expected validation input probe look delta in active camera log, actual {lines}")
    else:
        active_camera = active_camera_lines[0]
        camera_x = parse_named_float(active_camera, "x")
        camera_y = parse_named_float(active_camera, "y")
        camera_z = parse_named_float(active_camera, "z")
        camera_pitch = parse_named_float(active_camera, "pitch")
        camera_yaw = parse_named_float(active_camera, "yaw")
        if (
            snapshot_origin is None
            or snapshot_origin[0] is None
            or snapshot_origin[1] is None
            or snapshot_origin[2] is None
            or not close_to(camera_x - snapshot_origin[0], 1.942, 0.001)
            or not close_to(camera_y - snapshot_origin[1], 0.935, 0.001)
            or not close_to(camera_z - snapshot_origin[2], -1.118, 0.001)
            or not close_to(camera_pitch, -0.454720, 0.000001)
            or not close_to(camera_yaw, 0.209440, 0.000001)
        ):
            errors.append(f"{log_file}: expected validation input probe to move and rotate the live camera, actual {active_camera!r}")

    active_movement_lines = [
        line
        for line in lines
        if line.startswith("live_movement_frame frame=1 active=1")
    ]
    if not active_movement_lines or "speed=100.000" not in active_movement_lines[0] or "sprint=1" not in active_movement_lines[0]:
        errors.append(f"{log_file}: expected validation input probe sprint movement log, actual {lines}")

    active_tick_input_lines = [
        line
        for line in lines
        if line.startswith("live_client_tick_input frame=1 dt=0.016667 flags=31 controller=1")
    ]
    if (
        not active_tick_input_lines
        or "move=(1.000,1.000,1.000)" not in active_tick_input_lines[0]
        or "relative_mouse=1" not in active_tick_input_lines[0]
    ):
        errors.append(f"{log_file}: expected validation input probe in pre-tick host input snapshot, actual {lines}")
    elif snapshot_origin is not None:
        tick_camera = parse_camera_tuple(active_tick_input_lines[0])
        if (
            tick_camera is None
            or snapshot_origin[0] is None
            or snapshot_origin[1] is None
            or snapshot_origin[2] is None
            or not close_to(tick_camera[0] - snapshot_origin[0], 1.942, 0.001)
            or not close_to(tick_camera[1] - snapshot_origin[1], 0.935, 0.001)
            or not close_to(tick_camera[2] - snapshot_origin[2], -1.118, 0.001)
            or not close_to(tick_camera[3], -0.454720, 0.000001)
            or not close_to(tick_camera[4], 0.209440, 0.000001)
        ):
            errors.append(f"{log_file}: expected validation input probe camera in pre-tick host input snapshot, actual {active_tick_input_lines[0]!r}")

    if not any(
        line.startswith("live_chunk_view frame=1 origin=")
        and "source=old_arch_window_math authority=server" in line
        for line in lines
    ):
        errors.append(f"{log_file}: expected old-architecture chunk window view log, actual {lines}")

    chunk_view_intent_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_view_intent source=process_file ")
    ]
    if (
        not chunk_view_intent_lines
        or parse_named_int(chunk_view_intent_lines[0], "radius") is None
        or parse_named_int(chunk_view_intent_lines[0], "radius") < 1
    ):
        errors.append(f"{log_file}: expected client chunk-view intent to request a multi-column server stream, actual {chunk_view_intent_lines[0] if chunk_view_intent_lines else lines!r}")

    if not any(
        line.startswith("live_player_input_intent source=process_file ")
        and "frame=1 flags=31 controller=1 move=(1.000,1.000,1.000)" in line
        for line in lines
    ):
        errors.append(f"{log_file}: expected client player input intent file log, actual {lines}")

    if not any(
        line.startswith("live_block_interaction_intent source=process_file ")
        and "frame=1 commands=2 break=1 place=1" in line
        for line in lines
    ):
        errors.append(f"{log_file}: expected client block interaction intent file log, actual {lines}")

    command_matches = [
        COMMAND_PATTERN.match(line)
        for line in lines
        if line.startswith("live_client_command_enqueue kind=1 ")
    ]
    command_matches = [match for match in command_matches if match is not None]
    place_commands = [match for match in command_matches if match.group("edit") == "place"]
    break_commands = [match for match in command_matches if match.group("edit") == "break"]
    if len(place_commands) != 1 or len(break_commands) != 1:
        errors.append(f"{log_file}: expected one logged place command and one logged break command, actual {lines}")
    else:
        place = place_commands[0]
        break_command = break_commands[0]
        place_position = tuple(int(place.group(name)) for name in ("x", "y", "z"))
        break_position = tuple(int(break_command.group(name)) for name in ("x", "y", "z"))
        if place_position == break_position:
            errors.append(f"{log_file}: place command must target the adjacent air block, not the break target")
        if int(place.group("request")) >= int(break_command.group("request")):
            errors.append(f"{log_file}: place command must be submitted before break command for same-frame validation input")

    active_interaction_lines = [
        line
        for line in lines
        if line.startswith("live_interaction_frame frame=1 primary=1 secondary=1 command_enqueue_hook=active")
    ]
    if (
        not active_interaction_lines
        or "commands_enqueued=" not in active_interaction_lines[0]
        or "set_block=" not in active_interaction_lines[0]
        or "place=" not in active_interaction_lines[0]
        or "break=" not in active_interaction_lines[0]
    ):
        errors.append(f"{log_file}: expected per-frame command enqueue counters in active interaction log, actual {lines}")
    elif (
        "commands_enqueued=2" not in active_interaction_lines[0]
        or "set_block=2" not in active_interaction_lines[0]
        or "place=1" not in active_interaction_lines[0]
        or "break=1" not in active_interaction_lines[0]
    ):
        errors.append(f"{log_file}: expected one break and one place command from active interaction frame, actual {active_interaction_lines[0]!r}")

    try:
        module_descriptor_index = lines.index("basegame_module_descriptor=loaded")
        atlas_index = lines.index("basegame_atlas_manifest=loaded")
        animation_manifest_index = lines.index("basegame_animation_manifest=loaded")
        catalog_index = lines.index("basegame_block_catalog=loaded")
        atlas_texture_index = lines.index("basegame_atlas_texture=loaded")
        atlas_normal_texture_index = lines.index("basegame_atlas_normal_texture=loaded")
        atlas_specular_texture_index = lines.index("basegame_atlas_specular_texture=loaded")
        atlas_animation_texture_index = lines.index("basegame_atlas_animation_texture=loaded")
        initialize_index = lines.index("initialize=0")
        chunk_streaming_index = next(index for index, line in enumerate(lines) if line.startswith("live_chunk_streaming active=1 source=server_process"))
        snapshot_index = lines.index("world_blocks_snapshot=0")
        tick_input_index = next(index for index, line in enumerate(lines) if line.startswith("live_client_tick_input frame=1 dt=0.016667 flags=31 controller=1"))
        player_input_intent_index = next(index for index, line in enumerate(lines) if line.startswith("live_player_input_intent source=process_file "))
        block_interaction_intent_index = next(index for index, line in enumerate(lines) if line.startswith("live_block_interaction_intent source=process_file "))
        input_index = next(index for index, line in enumerate(lines) if line.startswith("live_input_frame frame=1"))
        camera_index = next(index for index, line in enumerate(lines) if line.startswith("live_camera_frame frame=1 active=1 mode=live_runtime"))
        movement_index = next(index for index, line in enumerate(lines) if line.startswith("live_movement_frame frame=1 active=1"))
        interaction_index = next(index for index, line in enumerate(lines) if line.startswith("live_interaction_frame frame=1 primary=1 secondary=1 command_enqueue_hook=active commands_enqueued="))
        presentation_frame_index = next(index for index, line in enumerate(lines) if line.startswith("live_presentation_frame frame=1"))
        mesh_plan_index = next(index for index, line in enumerate(lines) if line.startswith("live_chunk_mesh_plan frame=1 active=1 source=managed_presentation_pipeline"))
        mesh_upload_index = next(index for index, line in enumerate(lines) if line.startswith("live_chunk_mesh_upload frame=1 active=1 target=sdl_gpu"))
        drain_index = next(index for index, line in enumerate(lines) if line.startswith("presentation_updates_drained="))
        gpu_path_index = lines.index("gpu_render_path=SDL_GPU")
        material_index = lines.index("material_atlas_tiles_drawn=2")
        present_index = next(index for index, line in enumerate(lines) if line.startswith("presented_block_count="))
        shutdown_index = lines.index("shutdown=0")
    except (StopIteration, ValueError):
        return errors

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
        block_interaction_intent_index,
        tick_input_index,
        drain_index,
        mesh_plan_index,
        mesh_upload_index,
        input_index,
        camera_index,
        movement_index,
        interaction_index,
        presentation_frame_index,
        gpu_path_index,
        material_index,
        present_index,
        shutdown_index,
    )
    if list(expected_order) != sorted(expected_order):
        errors.append(
            f"{log_file}: expected bundled module descriptor before atlas manifest/animation manifest/catalog/material texture loads before initialize before live chunk/input/camera/presentation logs before drain before material atlas draw before presented blocks before shutdown, actual {lines}"
        )

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-file", required=True)
    args = parser.parse_args()

    errors = validate(pathlib.Path(args.log_file))
    if errors:
        for error in errors:
            print(f"client app launch probe log policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
