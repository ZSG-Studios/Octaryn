#!/usr/bin/env python3
import json


def latest_place_command(block_interaction_intent_path, errors):
    if block_interaction_intent_path is None:
        return None
    try:
        block_intent = json.loads(block_interaction_intent_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        errors.append(f"{block_interaction_intent_path}: block interaction intent is not valid JSON: {error}")
        return None

    expected_edit = None
    for command in block_intent.get("commands", []):
        if isinstance(command, dict) and command.get("block", 0) != 0:
            expected_edit = command
    if expected_edit is None:
        errors.append(f"{block_interaction_intent_path}: expected a place command in block interaction intent")
    return expected_edit


def validate_interaction_commands(block_interaction_intent_path):
    errors = []
    if block_interaction_intent_path is None:
        return errors
    try:
        block_intent = json.loads(block_interaction_intent_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        return [f"{block_interaction_intent_path}: block interaction intent is not valid JSON: {error}"]

    commands = [command for command in block_intent.get("commands", []) if isinstance(command, dict)]
    place_commands = [command for command in commands if command.get("block", 0) != 0]
    break_commands = [command for command in commands if command.get("block", 0) == 0]
    if len(place_commands) != 1 or len(break_commands) != 1:
        errors.append(f"{block_interaction_intent_path}: expected exactly one place and one break command")
        return errors

    place = place_commands[0]
    break_command = break_commands[0]
    place_edit = (place.get("editX"), place.get("editY"), place.get("editZ"))
    place_hit = (place.get("hitX"), place.get("hitY"), place.get("hitZ"))
    break_edit = (break_command.get("editX"), break_command.get("editY"), break_command.get("editZ"))
    if place_edit == break_edit:
        errors.append(f"{block_interaction_intent_path}: place command must target the adjacent air block, not the break target")
    if place_hit != break_edit:
        errors.append(f"{block_interaction_intent_path}: place command must carry the original hit block as its hit target")
    manhattan = sum(abs(int(a) - int(b)) for a, b in zip(place_edit, place_hit))
    if manhattan != 1:
        errors.append(f"{block_interaction_intent_path}: place edit must be adjacent to its hit block")
    return errors


def validate_world_blocks_file(world_blocks_path, block_interaction_intent_path):
    errors = []
    if not world_blocks_path.is_file():
        if block_interaction_intent_path is None:
            return errors
        return [f"{world_blocks_path}: bundled server did not persist authored block edits"]
    if world_blocks_path.stat().st_size == 0:
        return [f"{world_blocks_path}: initialized world block save is empty"]

    try:
        document = json.loads(world_blocks_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        return [f"{world_blocks_path}: initialized world block save is not valid JSON: {error}"]

    if document.get("version") != 1:
        errors.append(f"{world_blocks_path}: initialized world block save has unexpected version {document.get('version')!r}")
    blocks = document.get("blocks")
    if not isinstance(blocks, list):
        errors.append(f"{world_blocks_path}: initialized world block save must contain a blocks array")
        return errors
    errors.extend(validate_interaction_commands(block_interaction_intent_path))
    expected_edit = latest_place_command(block_interaction_intent_path, errors)
    if expected_edit is not None:
        persisted_blocks = [
            block
            for block in blocks
            if isinstance(block, dict)
            and block.get("x") == expected_edit.get("editX")
            and block.get("y") == expected_edit.get("editY")
            and block.get("z") == expected_edit.get("editZ")
        ]
        if not persisted_blocks or persisted_blocks[-1].get("block") != expected_edit.get("block"):
            errors.append(
                f"{world_blocks_path}: expected persisted block interaction edit "
                f"at ({expected_edit.get('editX')},{expected_edit.get('editY')},{expected_edit.get('editZ')})"
            )
    return errors


def validate_chunk_stream_file(chunk_stream_path, chunk_view_intent_path, player_input_intent_path, block_interaction_intent_path):
    errors = []
    if not chunk_stream_path.is_file():
        return [f"{chunk_stream_path}: bundled server did not write a chunk stream snapshot"]

    try:
        document = json.loads(chunk_stream_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        return [f"{chunk_stream_path}: chunk stream snapshot is not valid JSON: {error}"]

    if document.get("version") != 1:
        errors.append(f"{chunk_stream_path}: chunk stream snapshot has unexpected version {document.get('version')!r}")
    if document.get("source") != "server_process_chunk_stream":
        errors.append(f"{chunk_stream_path}: chunk stream snapshot has unexpected source {document.get('source')!r}")
    if document.get("epoch") != 1:
        errors.append(f"{chunk_stream_path}: chunk stream snapshot has unexpected epoch {document.get('epoch')!r}")
    columns = document.get("columns")
    blocks = document.get("blocks")
    window_events = document.get("windowEvents")
    window_load_count = document.get("windowLoadCount")
    window_preserve_count = document.get("windowPreserveCount")
    window_unload_count = document.get("windowUnloadCount")
    player_source = document.get("playerStateSource")
    player_control_mode = document.get("playerControlMode")
    expected_radius = None
    try:
        intent = json.loads(chunk_view_intent_path.read_text(encoding="utf-8"))
        expected_radius = intent.get("radius")
    except json.JSONDecodeError as error:
        errors.append(f"{chunk_view_intent_path}: chunk view intent is not valid JSON: {error}")
        intent = None

    if not isinstance(expected_radius, int) or expected_radius < 1:
        errors.append(f"{chunk_view_intent_path}: expected a multi-column chunk stream radius, actual {expected_radius!r}")
        expected_column_count = None
    else:
        expected_column_count = (expected_radius * 2 + 1) ** 2

    if not isinstance(columns, list) or expected_column_count is None or len(columns) != expected_column_count:
        errors.append(f"{chunk_stream_path}: expected {expected_column_count or 'multi'} streamed chunk columns")
    if not isinstance(blocks, list):
        errors.append(f"{chunk_stream_path}: expected streamed edit override blocks array")
    if not isinstance(window_events, list) or not window_events:
        errors.append(f"{chunk_stream_path}: expected server chunk-window streaming markers")
    if not isinstance(window_load_count, int) or window_load_count < 1:
        errors.append(f"{chunk_stream_path}: expected at least one server chunk-window load marker")
    if not isinstance(window_preserve_count, int) or window_preserve_count < 0:
        errors.append(f"{chunk_stream_path}: expected non-negative server chunk-window preserve marker count")
    if not isinstance(window_unload_count, int) or window_unload_count < 0:
        errors.append(f"{chunk_stream_path}: expected non-negative server chunk-window unload marker count")
    if player_source != "server_authority":
        errors.append(f"{chunk_stream_path}: expected server-authoritative player state source")
    if player_control_mode != "fly":
        errors.append(f"{chunk_stream_path}: expected fly-mode player state from input intent")
    for field, expected in (
        ("playerX", 1.942),
        ("playerY", 36.655),
        ("playerZ", -1.118),
        ("playerPitch", -0.454720),
        ("playerYaw", 0.209440),
    ):
        actual = document.get(field)
        if not isinstance(actual, (int, float)) or abs(float(actual) - expected) > 0.002:
            errors.append(f"{chunk_stream_path}: expected {field} near {expected}, actual {actual!r}")
    if player_input_intent_path is not None and not player_input_intent_path.is_file():
        errors.append(f"{player_input_intent_path}: expected client/server player input intent file")
    if block_interaction_intent_path is not None and not block_interaction_intent_path.is_file():
        errors.append(f"{block_interaction_intent_path}: expected client/server block interaction intent file")
    if block_interaction_intent_path is not None:
        expected_edit = latest_place_command(block_interaction_intent_path, errors)

        if expected_edit is None:
            return errors

        edited_blocks = [
            block
            for block in blocks or []
            if isinstance(block, dict)
            and block.get("x") == expected_edit.get("editX")
            and block.get("y") == expected_edit.get("editY")
            and block.get("z") == expected_edit.get("editZ")
        ]
        if not edited_blocks or edited_blocks[-1].get("block") != expected_edit.get("block"):
            errors.append(
                f"{chunk_stream_path}: expected server-applied block interaction edit "
                f"at ({expected_edit.get('editX')},{expected_edit.get('editY')},{expected_edit.get('editZ')})"
            )

    if intent is not None:
        if intent.get("hasPreviousWindow"):
            previous_radius = intent.get("previousRadius")
            previous_center_x = intent.get("previousCenterChunkX")
            previous_center_z = intent.get("previousCenterChunkZ")
            center_x = intent.get("centerChunkX")
            center_z = intent.get("centerChunkZ")
            if (
                not isinstance(previous_radius, int)
                or expected_radius is None
                or not isinstance(expected_radius, int)
                or not isinstance(previous_center_x, int)
                or not isinstance(previous_center_z, int)
                or not isinstance(center_x, int)
                or not isinstance(center_z, int)
            ):
                errors.append(f"{chunk_view_intent_path}: expected integer previous/current windows for chunk-window transition")
            else:
                current_chunks = {
                    (x, z)
                    for x in range(center_x - expected_radius, center_x + expected_radius + 1)
                    for z in range(center_z - expected_radius, center_z + expected_radius + 1)
                }
                previous_chunks = {
                    (x, z)
                    for x in range(previous_center_x - previous_radius, previous_center_x + previous_radius + 1)
                    for z in range(previous_center_z - previous_radius, previous_center_z + previous_radius + 1)
                }
                expected_unloads = len(previous_chunks - current_chunks)
                if window_unload_count != expected_unloads:
                    errors.append(f"{chunk_stream_path}: expected {expected_unloads} unload markers for chunk-window transition")
                has_unload_event = any(isinstance(event, dict) and event.get("kind") == "unload" for event in window_events or [])
                if expected_unloads > 0 and not has_unload_event:
                    errors.append(f"{chunk_stream_path}: expected an unload event for previous chunk window")
    return errors
