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


def run_bundled_server(entrypoint, payload_root, world_blocks_path, timeout_seconds):
    env = os.environ.copy()
    env["OCTARYN_SERVER_WORLD_BLOCKS_PATH"] = str(world_blocks_path)
    world_blocks_path.parent.mkdir(parents=True, exist_ok=True)
    if world_blocks_path.exists():
        world_blocks_path.unlink()
    for chunk_column_path in world_blocks_path.parent.glob("chunk_*.json"):
        chunk_column_path.unlink()

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


def validate(client_bundle_root, world_blocks_path, log_file, timeout_seconds):
    payload_root = client_bundle_root / PAYLOAD_DIR
    errors = []
    log_lines = [
        "client_server_app_launch_probe=begin",
        f"client_bundle_root={client_bundle_root}",
        f"payload_root={payload_root}",
        f"world_blocks_path={world_blocks_path}",
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

    errors.extend(validate_world_blocks_file(world_blocks_path))
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
    parser.add_argument("--log-file", required=True)
    parser.add_argument("--timeout-seconds", type=int, default=20)
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.client_bundle_root).resolve(),
        pathlib.Path(args.world_blocks_path).resolve(),
        pathlib.Path(args.log_file).resolve(),
        args.timeout_seconds)
    if errors:
        for error in errors:
            print(f"client server app readiness: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
