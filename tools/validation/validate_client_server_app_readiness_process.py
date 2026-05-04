#!/usr/bin/env python3
import os
import stat
import subprocess

from validate_client_server_app_readiness_intents import (
    write_block_interaction_intent,
    write_chunk_view_intent,
    write_player_input_intent,
)


PAYLOAD_DIR = "server"
SERVER_ENTRYPOINT_FILES = (
    "Octaryn.Server",
    "Octaryn.Server.exe",
)
READY_SIGNAL = "octaryn_server_ready=1"
SHUTDOWN_SIGNAL = "octaryn_server_shutdown=1"
REQUIRED_SERVER_LIVE_PREFIXES = (
    "server_live_startup args=",
    "server_live_world_loaded blocks=",
    "server_live_world_generation available=",
    "server_live_module_validation valid=1",
    "server_live_bundled_module valid=1",
    "server_live_player_spawn_align active=1",
    "server_live_activate active=1",
    "server_live_client_command_drain applied=",
    "server_live_tick frame=",
    "server_live_readiness ready=1",
    "server_live_player_input_intent active=1 source=process_file",
    "server_live_player_state frame=1 tick_input=1 authority=server",
    "server_live_block_interaction_intent active=1 source=process_file",
    "server_live_block_interaction_submit result=0 commands=2",
    "server_live_chunk_view_intent source=process_file",
    "server_live_chunk_window epoch=",
    "server_live_chunk_stream active=1 source=process_file",
)


def find_entrypoint(payload_root):
    for relative in SERVER_ENTRYPOINT_FILES:
        candidate = payload_root / relative
        if candidate.is_file():
            return candidate
    return None


def validate_entrypoint(entrypoint, errors):
    if entrypoint is None:
        errors.append("bundled server app has no launchable server entrypoint")
        return

    if entrypoint.name.endswith(".exe"):
        return

    mode = entrypoint.stat().st_mode
    if not mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH):
        errors.append(f"{entrypoint}: bundled server entrypoint is not executable")


def write_log(log_file, lines):
    log_file.parent.mkdir(parents=True, exist_ok=True)
    log_file.write_text("\n".join(lines) + "\n", encoding="utf-8")


def output_text(value):
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return value


def run_bundled_server(
    entrypoint,
    payload_root,
    world_blocks_path,
    chunk_view_intent_path,
    chunk_stream_path,
    player_input_intent_path,
    block_interaction_intent_path,
    write_default_intent,
    timeout_seconds,
):
    env = os.environ.copy()
    env["OCTARYN_SERVER_WORLD_BLOCKS_PATH"] = str(world_blocks_path)
    env["OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH"] = str(chunk_view_intent_path)
    env["OCTARYN_SERVER_CHUNK_STREAM_PATH"] = str(chunk_stream_path)
    env["OCTARYN_SERVER_PLAYER_SAVE_ROOT"] = str(world_blocks_path.parent)
    if player_input_intent_path is not None:
        env["OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH"] = str(player_input_intent_path)
    if block_interaction_intent_path is not None:
        env["OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH"] = str(block_interaction_intent_path)
    world_blocks_path.parent.mkdir(parents=True, exist_ok=True)
    if world_blocks_path.exists():
        world_blocks_path.unlink()
    if chunk_stream_path.exists():
        chunk_stream_path.unlink()
    for chunk_column_path in world_blocks_path.parent.glob("chunk_*.json"):
        chunk_column_path.unlink()
    for player_path in world_blocks_path.parent.glob("player_*.json"):
        player_path.unlink()
    if write_default_intent:
        write_chunk_view_intent(chunk_view_intent_path)
        if player_input_intent_path is not None:
            write_player_input_intent(player_input_intent_path)
        if block_interaction_intent_path is not None:
            write_block_interaction_intent(block_interaction_intent_path)

    return subprocess.run(
        [str(entrypoint)],
        cwd=payload_root,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout_seconds,
        check=False)
