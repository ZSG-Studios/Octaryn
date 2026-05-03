#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys


REQUIRED_LINES = (
    "window_show=0",
    "renderer_create=0",
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
    "material_atlas_tiles_drawn=2",
    "shutdown=0",
)
REQUIRED_PREFIXES = (
    "live_chunk_streaming active=0 source=world_blocks_path",
    "live_client_tick_input frame=1 dt=0.016667 flags=31 controller=1",
    "live_input_frame frame=1 active=1 move=(1.000,1.000,1.000)",
    "live_camera_frame frame=1 active=1 mode=live_runtime",
    "live_movement_frame frame=1 active=1",
    "live_interaction_frame frame=1 primary=1 secondary=1 command_enqueue_hook=active commands_enqueued=",
    "live_presentation_frame frame=1",
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


def close_to(value, expected, tolerance):
    return value is not None and abs(value - expected) <= tolerance


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

    loaded_blocks = parse_positive_count(lines, "world_blocks_loaded=")
    if not loaded_blocks or max(loaded_blocks) <= 1:
        errors.append(f"{log_file}: expected generated world block load, actual {lines}")

    surface_blocks = parse_positive_count(lines, "world_surface_blocks_applied=")
    if not surface_blocks or max(surface_blocks) <= 1:
        errors.append(f"{log_file}: expected generated world surface block snapshot, actual {lines}")

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

    atlas_tiles = parse_positive_count(lines, "atlas_tiles_drawn=")
    if not atlas_tiles or max(atlas_tiles) <= 1:
        errors.append(f"{log_file}: expected multiple atlas tiles drawn, actual {lines}")

    material_tiles = parse_positive_count(lines, "material_atlas_tiles_drawn=")
    if not material_tiles or max(material_tiles) != 2:
        errors.append(f"{log_file}: expected normal/specular material atlas tiles drawn, actual {lines}")

    presented_blocks = parse_positive_count(lines, "presented_block_count=")
    if not presented_blocks or max(presented_blocks) <= 1:
        errors.append(f"{log_file}: expected multiple presented blocks, actual {lines}")

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
            not close_to(camera_x, 0.474, 0.001)
            or not close_to(camera_y, 0.358, 0.001)
            or not close_to(camera_z, -0.306, 0.001)
            or not close_to(camera_pitch, -0.104720, 0.000001)
            or not close_to(camera_yaw, 0.209440, 0.000001)
        ):
            errors.append(f"{log_file}: expected validation input probe to move and rotate the live camera, actual {active_camera!r}")

    active_movement_lines = [
        line
        for line in lines
        if line.startswith("live_movement_frame frame=1 active=1")
    ]
    if not active_movement_lines or "speed=24.000" not in active_movement_lines[0] or "sprint=1" not in active_movement_lines[0]:
        errors.append(f"{log_file}: expected validation input probe sprint movement log, actual {lines}")

    active_tick_input_lines = [
        line
        for line in lines
        if line.startswith("live_client_tick_input frame=1 dt=0.016667 flags=31 controller=1")
    ]
    if (
        not active_tick_input_lines
        or "move=(1.000,1.000,1.000)" not in active_tick_input_lines[0]
        or "camera=(0.474,0.358,-0.306,-0.104720,0.209440)" not in active_tick_input_lines[0]
        or "relative_mouse=1" not in active_tick_input_lines[0]
    ):
        errors.append(f"{log_file}: expected validation input probe in pre-tick host input snapshot, actual {lines}")

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

    clear_pixels = parse_positive_count(lines, "rendered_clear_pixels=")
    if not clear_pixels or max(clear_pixels) <= 0:
        errors.append(f"{log_file}: expected visible clear pixels, actual {lines}")

    atlas_pixels = parse_positive_count(lines, "rendered_atlas_pixels=")
    if not atlas_pixels or max(atlas_pixels) <= 0:
        errors.append(f"{log_file}: expected visible atlas-sampled pixels, actual {lines}")

    normal_pixels = parse_positive_count(lines, "rendered_normal_atlas_pixels=")
    if not normal_pixels or max(normal_pixels) <= 0:
        errors.append(f"{log_file}: expected visible normal atlas material pixels, actual {lines}")

    specular_pixels = parse_positive_count(lines, "rendered_specular_atlas_pixels=")
    if not specular_pixels or max(specular_pixels) <= 0:
        errors.append(f"{log_file}: expected visible specular atlas material pixels, actual {lines}")

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
        chunk_streaming_index = next(index for index, line in enumerate(lines) if line.startswith("live_chunk_streaming active=0 source=world_blocks_path"))
        snapshot_index = lines.index("world_blocks_snapshot=0")
        tick_input_index = next(index for index, line in enumerate(lines) if line.startswith("live_client_tick_input frame=1 dt=0.016667 flags=31 controller=1"))
        input_index = next(index for index, line in enumerate(lines) if line.startswith("live_input_frame frame=1"))
        camera_index = next(index for index, line in enumerate(lines) if line.startswith("live_camera_frame frame=1 active=1 mode=live_runtime"))
        movement_index = next(index for index, line in enumerate(lines) if line.startswith("live_movement_frame frame=1 active=1"))
        interaction_index = next(index for index, line in enumerate(lines) if line.startswith("live_interaction_frame frame=1 primary=1 secondary=1 command_enqueue_hook=active commands_enqueued="))
        presentation_frame_index = next(index for index, line in enumerate(lines) if line.startswith("live_presentation_frame frame=1"))
        drain_index = next(index for index, line in enumerate(lines) if line.startswith("presentation_updates_drained="))
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
        tick_input_index,
        drain_index,
        input_index,
        camera_index,
        movement_index,
        interaction_index,
        presentation_frame_index,
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
