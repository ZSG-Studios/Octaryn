#!/usr/bin/env python3
import argparse
import json
import os
import pathlib
import stat
import subprocess
import sys


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
    "server_live_seed_spawn ",
    "server_live_activate active=1",
    "server_live_client_command_drain applied=",
    "server_live_tick frame=",
    "server_live_readiness ready=1",
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


def write_chunk_view_intent(intent_path):
    intent_path.parent.mkdir(parents=True, exist_ok=True)
    intent_path.write_text(
        json.dumps(
            {
                "version": 1,
                "epoch": 1,
                "centerChunkX": 0,
                "centerChunkZ": 0,
                "radius": 0,
                "hasPreviousWindow": True,
                "previousCenterChunkX": -1,
                "previousCenterChunkZ": 0,
                "previousRadius": 0,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def run_bundled_server(
    entrypoint,
    payload_root,
    world_blocks_path,
    chunk_view_intent_path,
    chunk_stream_path,
    write_default_intent,
    timeout_seconds,
):
    env = os.environ.copy()
    env["OCTARYN_SERVER_WORLD_BLOCKS_PATH"] = str(world_blocks_path)
    env["OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH"] = str(chunk_view_intent_path)
    env["OCTARYN_SERVER_CHUNK_STREAM_PATH"] = str(chunk_stream_path)
    world_blocks_path.parent.mkdir(parents=True, exist_ok=True)
    if world_blocks_path.exists():
        world_blocks_path.unlink()
    if chunk_stream_path.exists():
        chunk_stream_path.unlink()
    for chunk_column_path in world_blocks_path.parent.glob("chunk_*.json"):
        chunk_column_path.unlink()
    if write_default_intent:
        write_chunk_view_intent(chunk_view_intent_path)

    return subprocess.run(
        [str(entrypoint)],
        cwd=payload_root,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout_seconds,
        check=False)


def validate_world_blocks_file(world_blocks_path):
    errors = []
    if not world_blocks_path.is_file():
        return [f"{world_blocks_path}: bundled server did not initialize the world block save"]
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
    if len(blocks) <= 1:
        errors.append(f"{world_blocks_path}: initialized world block save must contain generated basegame terrain, not a single validation block")
    if not any(isinstance(block, dict) and block.get("y", 0) < 0 for block in blocks):
        errors.append(f"{world_blocks_path}: initialized world block save must include centered terrain below the origin")
    return errors


def validate_chunk_stream_file(chunk_stream_path, chunk_view_intent_path):
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
    if not isinstance(columns, list) or len(columns) != 1:
        errors.append(f"{chunk_stream_path}: expected one streamed spawn chunk column")
    if not isinstance(blocks, list) or len(blocks) <= 1024:
        errors.append(f"{chunk_stream_path}: expected generated streamed chunk blocks")
    elif not any(isinstance(block, dict) and block.get("y", 0) < 0 for block in blocks):
        errors.append(f"{chunk_stream_path}: streamed chunk blocks must include centered terrain below the origin")
    if not isinstance(window_events, list) or not window_events:
        errors.append(f"{chunk_stream_path}: expected server chunk-window streaming markers")
    if not isinstance(window_load_count, int) or window_load_count < 1:
        errors.append(f"{chunk_stream_path}: expected at least one server chunk-window load marker")
    if not isinstance(window_preserve_count, int) or window_preserve_count < 0:
        errors.append(f"{chunk_stream_path}: expected non-negative server chunk-window preserve marker count")
    if not isinstance(window_unload_count, int) or window_unload_count < 0:
        errors.append(f"{chunk_stream_path}: expected non-negative server chunk-window unload marker count")

    try:
        intent = json.loads(chunk_view_intent_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        errors.append(f"{chunk_view_intent_path}: chunk view intent is not valid JSON: {error}")
    else:
        if intent.get("hasPreviousWindow"):
            if window_unload_count != 1:
                errors.append(f"{chunk_stream_path}: expected one unload marker for camera chunk boundary transition")
            if not any(isinstance(event, dict) and event.get("kind") == "unload" for event in window_events or []):
                errors.append(f"{chunk_stream_path}: expected an unload event for previous chunk window")
    return errors


def validate(
    client_bundle_root,
    world_blocks_path,
    chunk_view_intent_path,
    chunk_stream_path,
    write_default_intent,
    log_file,
    timeout_seconds,
):
    payload_root = client_bundle_root / PAYLOAD_DIR
    errors = []
    log_lines = [
        "client_server_app_launch_probe=begin",
        f"client_bundle_root={client_bundle_root}",
        f"payload_root={payload_root}",
        f"world_blocks_path={world_blocks_path}",
        f"chunk_view_intent_path={chunk_view_intent_path}",
        f"chunk_stream_path={chunk_stream_path}",
    ]

    if not client_bundle_root.exists():
        errors.append(f"{client_bundle_root}: client bundle root is missing")
    if not payload_root.exists():
        errors.append(f"{payload_root}: bundled server app is missing")

    entrypoint = find_entrypoint(payload_root) if payload_root.exists() else None
    validate_entrypoint(entrypoint, errors)
    if entrypoint is not None:
        log_lines.append(f"entrypoint={entrypoint}")

    if errors:
        log_lines.append("client_server_app_launch_probe=failed_preflight")
        write_log(log_file, log_lines)
        return errors

    try:
        result = run_bundled_server(
            entrypoint,
            payload_root,
            world_blocks_path,
            chunk_view_intent_path,
            chunk_stream_path,
            write_default_intent,
            timeout_seconds)
    except subprocess.TimeoutExpired as error:
        log_lines.append(f"timeout_seconds={timeout_seconds}")
        log_lines.append(f"stdout={output_text(error.stdout)}")
        log_lines.append(f"stderr={output_text(error.stderr)}")
        log_lines.append("client_server_app_launch_probe=timeout")
        write_log(log_file, log_lines)
        return [f"{entrypoint}: bundled server readiness probe timed out after {timeout_seconds}s"]

    log_lines.append(f"exit_code={result.returncode}")
    log_lines.append(f"stdout={result.stdout}")
    log_lines.append(f"stderr={result.stderr}")
    if result.returncode != 0:
        log_lines.append("client_server_app_launch_probe=failed")
        write_log(log_file, log_lines)
        return [f"{entrypoint}: bundled server readiness probe exited with {result.returncode}"]

    stdout_lines = result.stdout.splitlines()
    if READY_SIGNAL not in stdout_lines:
        log_lines.append("client_server_app_launch_probe=missing_ready_signal")
        write_log(log_file, log_lines)
        return [f"{entrypoint}: bundled server readiness probe did not emit {READY_SIGNAL}"]
    if SHUTDOWN_SIGNAL not in stdout_lines:
        log_lines.append("client_server_app_launch_probe=missing_shutdown_signal")
        write_log(log_file, log_lines)
        return [f"{entrypoint}: bundled server readiness probe did not emit {SHUTDOWN_SIGNAL}"]
    missing_live_logs = [
        prefix
        for prefix in REQUIRED_SERVER_LIVE_PREFIXES
        if not any(line.startswith(prefix) for line in stdout_lines)
    ]
    if missing_live_logs:
        log_lines.append("client_server_app_launch_probe=missing_live_debug_logs")
        write_log(log_file, log_lines)
        return [f"{entrypoint}: bundled server readiness probe missing live debug log prefixes {missing_live_logs!r}"]

    errors.extend(validate_world_blocks_file(world_blocks_path))
    errors.extend(validate_chunk_stream_file(chunk_stream_path, chunk_view_intent_path))
    if errors:
        log_lines.append("client_server_app_launch_probe=failed_world_save")
        write_log(log_file, log_lines)
        return errors

    log_lines.append("client_server_app_launch_probe=passed")
    write_log(log_file, log_lines)
    return []


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-bundle-root", required=True)
    parser.add_argument("--world-blocks-path", required=True)
    parser.add_argument("--chunk-view-intent-path", required=True)
    parser.add_argument("--chunk-stream-path", required=True)
    parser.add_argument("--preserve-chunk-view-intent", action="store_true")
    parser.add_argument("--log-file", required=True)
    parser.add_argument("--timeout-seconds", type=int, default=20)
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.client_bundle_root).resolve(),
        pathlib.Path(args.world_blocks_path).resolve(),
        pathlib.Path(args.chunk_view_intent_path).resolve(),
        pathlib.Path(args.chunk_stream_path).resolve(),
        not args.preserve_chunk_view_intent,
        pathlib.Path(args.log_file).resolve(),
        args.timeout_seconds)
    if errors:
        for error in errors:
            print(f"client server app readiness: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
