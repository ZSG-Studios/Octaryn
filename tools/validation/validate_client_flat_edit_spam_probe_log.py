#!/usr/bin/env python3
import argparse
from pathlib import Path
import re
import sys

from client_app_launch_probe_log.parsing import parse_named_int, read_log_lines
from client_mesh_audit import validate_mesh_audit
from client_movement_frame_pacing import validate_frame_pacing


FLOAT_PATTERN = re.compile(r" (-?[a-zA-Z0-9_]+)=(-?\d+(?:\.\d+)?)")
POSITION_PATTERN = re.compile(
    r" pos=\((-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+)\)"
)


def parse_named_float(line, name):
    for match in FLOAT_PATTERN.finditer(line):
        if match.group(1) == name:
            return float(match.group(2))
    return None


def parse_position(line):
    match = POSITION_PATTERN.search(line)
    if match is None:
        return None
    return tuple(float(value) for value in match.groups())


def validate_edit_spam(log_file, lines, errors):
    interaction_lines = [
        line for line in lines if line.startswith("live_interaction_frame ")
    ]
    place_frames = [
        line for line in interaction_lines if (parse_named_int(line, "place") or 0) > 0
    ]
    break_frames = [
        line for line in interaction_lines if (parse_named_int(line, "break") or 0) > 0
    ]
    if len(place_frames) < 20 or len(break_frames) < 20:
        errors.append(
            f"{log_file}: expected flat edit spam to submit many place/break frames, "
            f"actual place={len(place_frames)} break={len(break_frames)}"
        )


def validate_draw_stability(log_file, lines, errors):
    draw_lines = [
        line
        for line in lines
        if line.startswith(
            "live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline active=1"
        )
    ]
    drawn = [parse_named_int(line, "chunks") for line in draw_lines]
    drawn = [value for value in drawn if value is not None]
    if len(drawn) < 120:
        errors.append(f"{log_file}: expected sustained world draw samples")
        return
    full_threshold = max(512, int(max(drawn, default=0) * 0.75))
    first_full_index = next(
        (index for index, value in enumerate(drawn) if value >= full_threshold),
        None,
    )
    if first_full_index is None:
        errors.append(
            f"{log_file}: world never reached sustained render-distance draw count, "
            f"max={max(drawn, default=0)}"
        )
        return
    warm = drawn[first_full_index:]
    collapse_threshold = max(16, int(full_threshold * 0.5))
    if min(warm, default=0) < collapse_threshold:
        errors.append(
            f"{log_file}: chunk draw count collapsed after reaching full stream, "
            f"min={min(warm, default=0)} threshold={collapse_threshold} "
            f"tail={warm[-20:]}"
        )

    validate_mesh_audit(log_file, lines, errors)


def validate_visual_terrain_coverage(log_file, lines, errors):
    audit_lines = [
        line for line in lines if line.startswith("live_terrain_visual_audit ")
    ]
    if len(audit_lines) < 4:
        errors.append(f"{log_file}: expected recurring terrain visual audit samples")
        return
    hole_lines = [
        line for line in audit_lines if (parse_named_int(line, "hole") or 0) != 0
    ]
    if hole_lines:
        errors.append(
            f"{log_file}: terrain visual audit saw sky-colored holes in the "
            f"expected terrain view, examples={hole_lines[:3]}"
        )


def validate_client_vertical_stability(log_file, lines, errors):
    terrain_lines = [
        line for line in lines if line.startswith("live_camera_terrain_state ")
    ]
    heights = [
        parse_named_float(line, "eye_above_surface") for line in terrain_lines
    ]
    heights = [value for value in heights if value is not None]
    if len(heights) < 8:
        errors.append(f"{log_file}: expected camera terrain-state samples")
        return
    deltas = [abs(right - left) for left, right in zip(heights, heights[1:])]
    if max(deltas, default=0.0) > 1.25:
        errors.append(
            f"{log_file}: camera vertical movement jittered during flat edit spam, "
            f"max_delta={max(deltas):.3f}"
        )


def validate_server_vertical_stability(server_log, lines, errors):
    player_lines = [
        line
        for line in lines
        if line.startswith("server_live_player_state ") and " tick_input=1 " in line
    ]
    positions = [parse_position(line) for line in player_lines]
    positions = [position for position in positions if position is not None]
    if len(positions) < 120:
        errors.append(f"{server_log}: expected sustained server movement samples")
        return
    y_values = [position[1] for position in positions]
    deltas = [abs(right - left) for left, right in zip(y_values, y_values[1:])]
    if max(deltas, default=0.0) > 1.25:
        errors.append(
            f"{server_log}: server vertical movement jumped during flat edit spam, "
            f"max_delta={max(deltas):.3f}"
        )
    x_values = [position[0] for position in positions]
    if max(x_values, default=0.0) - min(x_values, default=0.0) < 6.0:
        errors.append(f"{server_log}: expected sustained full-speed flat movement")
    tiny_steps = 0
    worst_tiny_run = 0
    for left, right in zip(positions, positions[1:]):
        dx = abs(right[0] - left[0])
        dz = abs(right[2] - left[2])
        if dx + dz < 0.001:
            tiny_steps += 1
            worst_tiny_run = max(worst_tiny_run, tiny_steps)
        else:
            tiny_steps = 0
    if worst_tiny_run > 12:
        errors.append(
            f"{server_log}: server movement stalled while input stayed active, "
            f"still_ticks={worst_tiny_run}"
        )


def validate_no_floor_breaks(server_log, lines, errors):
    floor_breaks = [
        line
        for line in lines
        if "server_live_block_command rejected=0 " in line
        and " edit=break " in line
        and ",34," in line
    ]
    if floor_breaks:
        errors.append(
            f"{server_log}: flat edit spam broke walk surface blocks, "
            f"examples={floor_breaks[:3]}"
        )


def validate(log_file, server_log):
    errors = []
    lines = read_log_lines(log_file)
    validate_frame_pacing(log_file, lines, errors)
    validate_edit_spam(log_file, lines, errors)
    validate_draw_stability(log_file, lines, errors)
    validate_visual_terrain_coverage(log_file, lines, errors)
    validate_client_vertical_stability(log_file, lines, errors)
    if server_log is None or not server_log.exists():
        errors.append(f"{server_log}: expected bundled server live log")
    else:
        validate_server_vertical_stability(
            server_log, read_log_lines(server_log), errors
        )
        validate_no_floor_breaks(server_log, read_log_lines(server_log), errors)
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-file", required=True, type=Path)
    parser.add_argument("--server-log", type=Path)
    args = parser.parse_args()
    errors = validate(args.log_file, args.server_log)
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
