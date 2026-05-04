#!/usr/bin/env python3
import argparse
import pathlib
import sys
from subprocess import TimeoutExpired

from validate_client_server_app_readiness_checks import (
    validate_chunk_stream_file,
    validate_world_blocks_file,
)
from validate_client_server_app_readiness_process import (
    PAYLOAD_DIR,
    READY_SIGNAL,
    REQUIRED_SERVER_LIVE_PREFIXES,
    SHUTDOWN_SIGNAL,
    find_entrypoint,
    output_text,
    run_bundled_server,
    validate_entrypoint,
    write_log,
)


def validate(
    client_bundle_root,
    world_blocks_path,
    chunk_view_intent_path,
    chunk_stream_path,
    player_input_intent_path,
    block_interaction_intent_path,
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
        f"player_input_intent_path={player_input_intent_path}",
        f"block_interaction_intent_path={block_interaction_intent_path}",
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
            player_input_intent_path,
            block_interaction_intent_path,
            write_default_intent,
            timeout_seconds)
    except TimeoutExpired as error:
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
    if block_interaction_intent_path is not None:
        required_authority_logs = (
            "server_live_block_command rejected=0 kind=SetBlock request=",
            "server_live_client_command_drain applied=2 pending=0",
        )
        missing_authority_logs = [
            prefix
            for prefix in required_authority_logs
            if not any(line.startswith(prefix) for line in stdout_lines)
        ]
        if not any(
            line.startswith("server_live_block_command rejected=0 kind=SetBlock request=")
            and " edit=break applied=1 changed=1 " in line
            for line in stdout_lines
        ):
            missing_authority_logs.append("server_live_block_command edit=break applied=1 changed=1")
        if not any(
            line.startswith("server_live_block_command rejected=0 kind=SetBlock request=")
            and " edit=place applied=1 changed=1 " in line
            for line in stdout_lines
        ):
            missing_authority_logs.append("server_live_block_command edit=place applied=1 changed=1")
        if missing_authority_logs:
            log_lines.append("client_server_app_launch_probe=missing_block_authority_logs")
            write_log(log_file, log_lines)
            return [f"{entrypoint}: bundled server readiness probe missing block authority logs {missing_authority_logs!r}"]

    errors.extend(validate_world_blocks_file(world_blocks_path, block_interaction_intent_path))
    errors.extend(validate_chunk_stream_file(chunk_stream_path, chunk_view_intent_path, player_input_intent_path, block_interaction_intent_path))
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
    parser.add_argument("--player-input-intent-path")
    parser.add_argument("--block-interaction-intent-path")
    parser.add_argument("--preserve-chunk-view-intent", action="store_true")
    parser.add_argument("--log-file", required=True)
    parser.add_argument("--timeout-seconds", type=int, default=20)
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.client_bundle_root).resolve(),
        pathlib.Path(args.world_blocks_path).resolve(),
        pathlib.Path(args.chunk_view_intent_path).resolve(),
        pathlib.Path(args.chunk_stream_path).resolve(),
        pathlib.Path(args.player_input_intent_path).resolve() if args.player_input_intent_path else None,
        pathlib.Path(args.block_interaction_intent_path).resolve() if args.block_interaction_intent_path else None,
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
