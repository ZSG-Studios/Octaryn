#!/usr/bin/env python3
import argparse
import math
from pathlib import Path
import re
import sys

from client_app_launch_probe_log.parsing import (
    COMMAND_PATTERN,
    parse_camera_tuple,
    parse_named_int,
    read_log_lines,
)
from client_movement_frame_pacing import validate_frame_pacing


CENTER_PATTERN = re.compile(r" center=\((-?\d+),(-?\d+)\)")
POSITION_PATTERN = re.compile(r" pos=\((-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+)\)")
NAMED_FLOAT_PATTERN = re.compile(r" (?P<name>[a-zA-Z0-9_]+)=(-?\d+(?:\.\d+)?)")

TERRAIN_MAX_Y = 255
MOVEMENT_PROBE_EYE_HEIGHT = 2.72
WALK_EYE_HEIGHT_MIN = 1.25
WALK_EYE_HEIGHT_MAX = 4.75
SERVER_COLLISION_RADIUS = 0.3


def uint32(value):
    return value & 0xFFFFFFFF


def rotl32(value, amount):
    return uint32((value << amount) | (value >> (32 - amount)))


def hash_noise(x, z, seed_offset):
    value = uint32(1337 + seed_offset)
    value = uint32(value ^ uint32(x * 0x9E3779B9))
    value = rotl32(value, 13)
    value = uint32(value ^ uint32(z * 0x85EBCA6B))
    value = uint32(value ^ (value >> 16))
    value = uint32(value * 0x7FEB352D)
    value = uint32(value ^ (value >> 15))
    value = uint32(value * 0x846CA68B)
    value = uint32(value ^ (value >> 16))
    return value / 4294967295.0 * 2.0 - 1.0


def lerp(start, end, amount):
    return start + (end - start) * amount


def smooth_step(value):
    return value * value * (3.0 - 2.0 * value)


def smooth_value_noise(x, z, seed_offset):
    x0 = math.floor(x)
    z0 = math.floor(z)
    tx = smooth_step(x - x0)
    tz = smooth_step(z - z0)
    a = lerp(hash_noise(x0, z0, seed_offset), hash_noise(x0 + 1, z0, seed_offset), tx)
    b = lerp(hash_noise(x0, z0 + 1, seed_offset), hash_noise(x0 + 1, z0 + 1, seed_offset), tx)
    return lerp(a, b, tz)


def sample_fbm(world_x, world_z, frequency, octaves, seed_offset):
    amplitude = 1.0
    amplitude_total = 0.0
    value = 0.0
    x = float(world_x) * frequency
    z = float(world_z) * frequency
    for octave in range(octaves):
        value += smooth_value_noise(x, z, seed_offset + octave * 9973) * amplitude
        amplitude_total += amplitude
        amplitude *= 0.5
        x *= 2.0
        z *= 2.0
    return value / amplitude_total if amplitude_total > 0.0 else 0.0


def terrain_surface_y(world_x, world_z):
    height = max(sample_fbm(world_x, world_z, 0.005, 6, 0) * 50.0, 0.0)
    height = math.pow(height, 1.3) + 30.0
    height = max(0.0, min(height, float(TERRAIN_MAX_Y)))
    if height < 40.0:
        height += sample_fbm(-world_x, world_z, 0.01, 6, 101) * 12.0
    return math.ceil(height)


def terrain_footprint_surface_options(world_x, world_z):
    # Logs print coordinates to 3 decimals, so values on block boundaries can
    # round across the floor() used by runtime terrain checks.
    x_options = {math.floor(world_x + offset) for offset in (-0.001, 0.0, 0.001)}
    z_options = {math.floor(world_z + offset) for offset in (-0.001, 0.0, 0.001)}
    return {
        max(
            terrain_surface_y(x + dx, z + dz)
            for dx in range(-1, 2)
            for dz in range(-1, 2)
        )
        for x in x_options
        for z in z_options
    }

def terrain_column_surface_options(world_x, world_z):
    x_options = {math.floor(world_x + offset) for offset in (-0.001, 0.0, 0.001)}
    z_options = {math.floor(world_z + offset) for offset in (-0.001, 0.0, 0.001)}
    return {terrain_surface_y(x, z) for x in x_options for z in z_options}


def terrain_collision_surface_options(world_x, world_z):
    x_options = {
        math.floor(world_x - SERVER_COLLISION_RADIUS + 0.001),
        math.floor(world_x + SERVER_COLLISION_RADIUS - 0.001),
    }
    z_options = {
        math.floor(world_z - SERVER_COLLISION_RADIUS + 0.001),
        math.floor(world_z + SERVER_COLLISION_RADIUS - 0.001),
    }
    return {terrain_surface_y(x, z) for x in x_options for z in z_options}

def parse_center(line):
    match = CENTER_PATTERN.search(line)
    if not match:
        return None
    return tuple(int(value) for value in match.groups())


def parse_position(line):
    match = POSITION_PATTERN.search(line)
    if not match:
        return None
    return tuple(float(value) for value in match.groups())


def parse_named_float(line, name):
    for match in NAMED_FLOAT_PATTERN.finditer(line):
        if match.group("name") == name:
            return float(match.group(2))
    return None


def active_tick_inputs(lines):
    return [
        line
        for line in lines
        if line.startswith("live_client_tick_input ") and " active=0" not in line
    ]


def active_world_draws(lines):
    return [
        line
        for line in lines
        if line.startswith("live_world_mesh_draw frame_source=sdl_gpu_shader_pipeline active=1")
    ]


def validate_movement(log_file, lines, errors):
    tick_lines = active_tick_inputs(lines)
    moving_frames = []
    moved_positions = set()
    for line in tick_lines:
        frame = parse_named_int(line, "frame")
        camera = parse_camera_tuple(line)
        if frame is None or camera is None:
            continue
        if "move=(0.000,0.000,0.000)" not in line:
            moving_frames.append(frame)
        moved_positions.add(tuple(round(value, 1) for value in camera[:3]))

    if len(moving_frames) < 120 or max(moving_frames, default=0) < 300:
        errors.append(f"{log_file}: expected sustained automated player movement through frame 300, actual moving frames={moving_frames[:8]}...{moving_frames[-8:]}")
    if len(moved_positions) < 16:
        errors.append(f"{log_file}: expected player camera to traverse many positions, actual distinct rounded positions={len(moved_positions)}")

    terrain_errors = []
    for line in tick_lines:
        camera = parse_camera_tuple(line)
        if camera is None:
            continue
        surfaces = terrain_footprint_surface_options(camera[0], camera[2])
        expected_options = [surface + MOVEMENT_PROBE_EYE_HEIGHT for surface in surfaces]
        if not any(abs(camera[1] - expected_y) <= 1.05 for expected_y in expected_options):
            closest = min(expected_options, key=lambda expected_y: abs(camera[1] - expected_y))
            terrain_errors.append((camera, sorted(surfaces), closest))
            if len(terrain_errors) >= 5:
                break
    if terrain_errors:
        errors.append(f"{log_file}: movement probe camera left terrain-follow height, examples={terrain_errors}")


def validate_chunk_centers(log_file, lines, errors):
    intent_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_view_intent source=process_file ")
    ]
    centers = [center for center in (parse_center(line) for line in intent_lines) if center is not None]
    distinct_centers = set(centers)
    if len(distinct_centers) < 3:
        errors.append(f"{log_file}: expected movement probe to drive at least three chunk-view centers, actual centers={centers}")
    if len(intent_lines) < 8:
        errors.append(f"{log_file}: expected repeated chunk-view intent updates during movement, actual {len(intent_lines)}")
    center_jumps = []
    for previous, current in zip(centers, centers[1:]):
        dx = abs(current[0] - previous[0])
        dz = abs(current[1] - previous[1])
        if dx > 1 or dz > 1:
            center_jumps.append((previous, current))
    if center_jumps:
        errors.append(f"{log_file}: chunk-view centers jumped instead of tracking movement continuously, examples={center_jumps[:5]}")


def validate_stream_batches(log_file, lines, errors):
    batch_lines = [
        line
        for line in lines
        if line.startswith("live_server_stream_mesh_batch ")
        and " active=1 source=server_seed_memory" in line
    ]
    if len(batch_lines) < 8:
        errors.append(f"{log_file}: expected repeated bounded stream mesh batches during movement, actual {batch_lines}")
        return

    stalled_batches = []
    clear_only_batches = []
    epoch_next_entry = {}
    discontinuities = []
    for line in batch_lines:
        epoch = parse_named_int(line, "epoch")
        first_entry = parse_named_int(line, "first_entry")
        next_entry = parse_named_int(line, "next_entry")
        processed = parse_named_int(line, "processed")
        build_columns = parse_named_int(line, "build_columns")
        clear_columns = parse_named_int(line, "clear_columns")
        if processed is None or processed < 1 or processed > 128:
            stalled_batches.append(line)
        if first_entry is not None and next_entry is not None and processed is not None:
            if next_entry - first_entry != processed:
                discontinuities.append(line)
            if (
                epoch is not None
                and first_entry != 0
                and epoch in epoch_next_entry
                and first_entry != epoch_next_entry[epoch]
            ):
                discontinuities.append(line)
            if epoch is not None:
                epoch_next_entry[epoch] = next_entry
        if clear_columns is not None and clear_columns > 0 and (build_columns is None or build_columns <= 0):
            clear_only_batches.append(line)

    if stalled_batches:
        errors.append(f"{log_file}: expected every movement stream batch to process 1..128 entries, actual {stalled_batches}")
    if discontinuities:
        errors.append(f"{log_file}: stream mesh batches skipped or repeated plan entries, examples={discontinuities[:5]}")
    if clear_only_batches:
        errors.append(f"{log_file}: movement stream produced clear-only unload batches that can make holes, actual {clear_only_batches}")


def validate_retained_and_drawn_world(log_file, lines, errors):
    retained_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_mesh_retained ") and " active=1" in line
    ]
    visible_counts = [
        value
        for value in (parse_named_int(line, "visible_chunks") for line in retained_lines)
        if value is not None
    ]
    if len(visible_counts) < 4 or max(visible_counts, default=0) < 64:
        errors.append(f"{log_file}: expected retained chunk mesh population to grow under movement, actual {visible_counts}")
    if len(visible_counts) >= 12 and min(visible_counts[-8:]) < 64:
        errors.append(f"{log_file}: retained mesh population collapsed near probe end, tail={visible_counts[-8:]}")

    draw_lines = active_world_draws(lines)
    drawn_chunks = [
        value
        for value in (parse_named_int(line, "chunks") for line in draw_lines)
        if value is not None
    ]
    drawn_faces = [
        value
        for value in (parse_named_int(line, "opaque_faces") for line in draw_lines)
        if value is not None
    ]
    if not drawn_chunks or max(drawn_chunks) < 16 or max(drawn_faces, default=0) <= 1:
        errors.append(f"{log_file}: expected visible world draw counters during movement, actual chunks={drawn_chunks} faces={drawn_faces}")
    if len(drawn_chunks) > 20 and min(drawn_chunks[-20:]) <= 0:
        errors.append(f"{log_file}: world draw collapsed to zero chunks after movement warmup, actual tail={drawn_chunks[-20:]}")
    if len(drawn_chunks) > 60 and min(drawn_chunks[-30:]) < 8:
        errors.append(f"{log_file}: drawn chunk count collapsed near probe end, tail={drawn_chunks[-30:]}")


def validate_interactions(log_file, lines, errors):
    if not any(line.startswith("live_block_interaction_intent ") for line in lines):
        errors.append(f"{log_file}: expected movement probe to exercise block interaction decisions")
    commands = [
        COMMAND_PATTERN.match(line)
        for line in lines
        if line.startswith("live_client_command_enqueue kind=1 ")
    ]
    commands = [match for match in commands if match is not None]
    if commands:
        edits = {match.group("edit") for match in commands}
        if edits != {"break", "place"}:
            errors.append(f"{log_file}: expected both break and place commands when interaction raycasts hit, actual edits={sorted(edits)}")


def validate_server_log(server_log, errors):
    if server_log is None:
        return
    if not server_log.exists():
        errors.append(f"{server_log}: expected bundled server live log from movement probe")
        return

    lines = read_log_lines(server_log)
    validate_server_authority_movement(server_log, lines, errors)
    validate_server_chunk_windows(server_log, lines, errors)
    validate_server_metadata_stream(server_log, lines, errors)
    validate_server_interactions(server_log, lines, errors)


def validate_server_authority_movement(server_log, lines, errors):
    player_lines = [
        line
        for line in lines
        if line.startswith("server_live_player_state ") and " tick_input=1 " in line
    ]
    frames = [parse_named_int(line, "frame") for line in player_lines]
    frames = [frame for frame in frames if frame is not None]
    positions = set()
    late_positions = set()
    tail_positions = set()
    terrain_errors = []
    fly_lines = []
    grounded_frames = []
    for line in player_lines:
        position = parse_position(line)
        if position:
            positions.add(tuple(round(value, 1) for value in position))
            frame = parse_named_int(line, "frame")
            if frame is not None and frame >= 600:
                late_positions.add(tuple(round(value, 1) for value in position))
            if frame is not None and frame >= 1080:
                tail_positions.add(tuple(round(value, 2) for value in position))
            surfaces = terrain_collision_surface_options(position[0], position[2])
            eye_options = [position[1] - surface for surface in surfaces]
            if not any(WALK_EYE_HEIGHT_MIN <= eye <= WALK_EYE_HEIGHT_MAX for eye in eye_options):
                closest = min(eye_options, key=lambda eye: abs(eye - MOVEMENT_PROBE_EYE_HEIGHT))
                terrain_errors.append((position, sorted(surfaces), round(closest, 3)))
        if " mode=fly " in line:
            fly_lines.append(line)
        if " ground=1 " in line:
            frame = parse_named_int(line, "frame")
            if frame is not None:
                grounded_frames.append(frame)

    if max(frames, default=0) < 300 or len(positions) < 16:
        errors.append(f"{server_log}: expected server-authoritative movement through frame 300, actual frames={frames[:8]}...{frames[-8:]} positions={len(positions)}")
    if max(frames, default=0) >= 900 and len(late_positions) < 8:
        errors.append(f"{server_log}: expected server walking movement to keep progressing after frame 600, late_positions={len(late_positions)}")
    if max(frames, default=0) >= 1200 and len(tail_positions) < 6:
        errors.append(f"{server_log}: expected server walking movement to keep progressing through the final 120 frames, tail_positions={len(tail_positions)}")
    if not any(" authority=server mode=walk " in line for line in player_lines):
        errors.append(f"{server_log}: expected movement probe to use server-authoritative walking movement")
    if fly_lines:
        errors.append(f"{server_log}: movement probe must not use fly mode, examples={fly_lines[:3]}")
    if not grounded_frames or max(grounded_frames) < 180:
        errors.append(f"{server_log}: expected walking probe to establish ground contact while moving, frames={grounded_frames[:8]}...{grounded_frames[-8:]}")
    if terrain_errors:
        errors.append(f"{server_log}: server player moved outside terrain-following walk bounds, examples={terrain_errors[:5]}")


def validate_server_chunk_windows(server_log, lines, errors):
    window_lines = [line for line in lines if line.startswith("server_live_chunk_window ")]
    centers = [center for center in (parse_center(line) for line in window_lines) if center is not None]
    loads = [parse_named_int(line, "load") for line in window_lines]
    unloads = [parse_named_int(line, "unload") for line in window_lines]
    loads = [value for value in loads if value is not None]
    unloads = [value for value in unloads if value is not None]
    if len(set(centers)) < 6:
        errors.append(f"{server_log}: expected server chunk windows to follow several movement centers, actual centers={centers}")
    if max(loads, default=0) <= 0 or max(unloads, default=0) <= 0:
        errors.append(f"{server_log}: expected server chunk windows to both load and unload during movement, actual load={loads} unload={unloads}")
    if not any(parse_named_int(line, "radius") == 32 for line in window_lines):
        errors.append(f"{server_log}: expected movement stream to reach radius 32 server windows")
    validate_server_window_accounting(server_log, window_lines, errors)


def validate_server_window_accounting(server_log, window_lines, errors):
    previous_center = None
    previous_radius = None
    previous_unload = 0
    mismatches = []
    jumps = []
    for line in window_lines:
        center = parse_center(line)
        radius = parse_named_int(line, "radius")
        load = parse_named_int(line, "load")
        preserve = parse_named_int(line, "preserve")
        unload = parse_named_int(line, "unload")
        if center is None or radius is None or load is None or preserve is None or unload is None:
            continue
        if previous_center is not None and previous_radius is not None:
            dx = abs(center[0] - previous_center[0])
            dz = abs(center[1] - previous_center[1])
            if radius == previous_radius == 32 and (dx > 1 or dz > 1):
                jumps.append((previous_center, center))
        total_columns = (radius * 2 + 1) * (radius * 2 + 1)
        if load + preserve != total_columns:
            mismatches.append(line)
        if radius == previous_radius and (unload > 0 or previous_unload > 0) and load != unload:
            mismatches.append(line)
        previous_center = center
        previous_radius = radius
        previous_unload = unload
    if jumps:
        errors.append(f"{server_log}: server chunk windows jumped instead of moving continuously, examples={jumps[:5]}")
    if mismatches:
        errors.append(f"{server_log}: chunk window load/preserve/unload accounting mismatch, examples={mismatches[:3]}")


def validate_server_metadata_stream(server_log, lines, errors):
    stream_lines = [
        line
        for line in lines
        if line.startswith("server_live_chunk_stream active=1 source=process_file ")
    ]
    if len(stream_lines) < 8:
        errors.append(f"{server_log}: expected repeated server chunk stream writes during movement")
    if not any(" metadata_only=1 " in line and " blocks=0 " in line for line in stream_lines):
        errors.append(f"{server_log}: expected seed terrain stream to stay metadata-only with zero block records")
    if not any(parse_named_int(line, "columns") == 4225 for line in stream_lines):
        errors.append(f"{server_log}: expected server stream to reach full radius-32 column count")


def validate_server_interactions(server_log, lines, errors):
    interaction_lines = [
        line
        for line in lines
        if line.startswith("server_live_block_interaction_intent active=1 ")
    ]
    if len(interaction_lines) < 2:
        errors.append(f"{server_log}: expected repeated block interaction command submissions during movement")
    submitted_break = any(parse_named_int(line, "break") == 1 for line in interaction_lines)
    submitted_place = any(parse_named_int(line, "place") == 1 for line in interaction_lines)
    if not submitted_break or not submitted_place:
        errors.append(f"{server_log}: expected movement probe interactions to submit both break and place commands")


def validate_probe_swapchain_size(log_file, lines, errors):
    acquire_lines = [line for line in lines if line.startswith("gpu_swapchain_acquired ")]
    if not acquire_lines:
        errors.append(f"{log_file}: expected swapchain acquisition dimensions")
        return
    width = parse_named_int(acquire_lines[0], "width")
    height = parse_named_int(acquire_lines[0], "height")
    if width is None or height is None:
        errors.append(f"{log_file}: expected parseable swapchain acquisition dimensions")
    elif width * height > 1920 * 1080:
        errors.append(
            f"{log_file}: movement probe must use controlled windowed settings, actual swapchain={width}x{height}"
        )


def validate_log(log_file, server_log):
    lines = read_log_lines(log_file)
    errors = []
    validate_probe_swapchain_size(log_file, lines, errors)
    validate_movement(log_file, lines, errors)
    validate_chunk_centers(log_file, lines, errors)
    validate_stream_batches(log_file, lines, errors)
    validate_retained_and_drawn_world(log_file, lines, errors)
    validate_frame_pacing(log_file, lines, errors)
    validate_interactions(log_file, lines, errors)
    validate_server_log(server_log, errors)
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-file", required=True, type=Path)
    parser.add_argument("--server-log", type=Path)
    args = parser.parse_args()
    errors = validate_log(args.log_file, args.server_log)
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
