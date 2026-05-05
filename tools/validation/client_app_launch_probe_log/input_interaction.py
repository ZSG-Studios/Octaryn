from .parsing import (
    COMMAND_PATTERN,
    close_to,
    parse_camera_tuple,
    parse_named_float,
    parse_named_int,
)


def validate_input_and_interaction(log_file, lines, errors):
    snapshot_origin = _validate_snapshot_origin(log_file, lines, errors)
    _validate_camera_and_movement(log_file, lines, errors, snapshot_origin)
    _validate_live_intents(log_file, lines, errors)
    _validate_commands(log_file, lines, errors)


def _validate_snapshot_origin(log_file, lines, errors):
    snapshot_origin_lines = [
        line
        for line in lines
        if line.startswith("snapshot_camera_origin x=")
    ]
    if not snapshot_origin_lines:
        errors.append(f"{log_file}: expected snapshot-relative camera origin, actual {lines}")
        return None

    return (
        parse_named_float(snapshot_origin_lines[0], "x"),
        parse_named_float(snapshot_origin_lines[0], "y"),
        parse_named_float(snapshot_origin_lines[0], "z"),
    )


def _validate_camera_and_movement(log_file, lines, errors, snapshot_origin):
    active_camera_lines = [
        line
        for line in lines
        if line.startswith("live_camera_frame frame=1 active=1 mode=live_runtime")
    ]
    if not active_camera_lines or "look=(-6.000,12.000)" not in active_camera_lines[0]:
        errors.append(f"{log_file}: expected validation input probe look delta in active camera log, actual {lines}")
    else:
        _validate_active_camera(log_file, errors, snapshot_origin, active_camera_lines[0])

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
        _validate_tick_camera(log_file, errors, snapshot_origin, active_tick_input_lines[0])


def _validate_active_camera(log_file, errors, snapshot_origin, active_camera):
    camera_x = parse_named_float(active_camera, "x")
    camera_y = parse_named_float(active_camera, "y")
    camera_z = parse_named_float(active_camera, "z")
    camera_pitch = parse_named_float(active_camera, "pitch")
    camera_yaw = parse_named_float(active_camera, "yaw")
    camera_far = parse_named_float(active_camera, "far")
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
        or not close_to(camera_far, 4096.0, 0.001)
    ):
        errors.append(f"{log_file}: expected validation input probe to move and rotate the live 32-distance camera with a render-distance far plane, actual {active_camera!r}")


def _validate_tick_camera(log_file, errors, snapshot_origin, active_tick_input):
    tick_camera = parse_camera_tuple(active_tick_input)
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
        errors.append(f"{log_file}: expected validation input probe camera in pre-tick host input snapshot, actual {active_tick_input!r}")


def _validate_live_intents(log_file, lines, errors):
    if not any(
        line.startswith("live_chunk_view frame=1 origin=")
        and "width=65 radius=32" in line
        and "source=render_distance_radius authority=server" in line
        for line in lines
    ):
        errors.append(f"{log_file}: expected 32-distance chunk window view log, actual {lines}")

    chunk_view_intent_lines = [
        line
        for line in lines
        if line.startswith("live_chunk_view_intent source=process_file ")
    ]
    if (
        not chunk_view_intent_lines
        or parse_named_int(chunk_view_intent_lines[0], "radius") is None
        or parse_named_int(chunk_view_intent_lines[0], "radius") < 1
        or parse_named_int(chunk_view_intent_lines[0], "target_radius") != 32
    ):
        errors.append(f"{log_file}: expected client chunk-view intent to target the 32-distance server stream, actual {chunk_view_intent_lines[0] if chunk_view_intent_lines else lines!r}")

    if not any(
        line.startswith("live_player_input_intent source=process_file ")
        and "frame=1 flags=31 controller=1 move=(1.000,1.000,1.000)" in line
        for line in lines
    ):
        errors.append(f"{log_file}: expected client player input intent file log, actual {lines}")

    if not any(line.startswith("live_block_interaction_intent ") for line in lines):
        errors.append(f"{log_file}: expected client block interaction decision log, actual {lines}")


def _validate_commands(log_file, lines, errors):
    command_matches = [
        COMMAND_PATTERN.match(line)
        for line in lines
        if line.startswith("live_client_command_enqueue kind=1 ")
    ]
    command_matches = [match for match in command_matches if match is not None]
    if not command_matches:
        if not any(line == "live_block_interaction_intent active=0 reason=raycast_miss" for line in lines):
            errors.append(f"{log_file}: expected commands or explicit raycast miss, actual {lines}")
        return

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
