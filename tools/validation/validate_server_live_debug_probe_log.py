#!/usr/bin/env python3
import argparse
import pathlib
import sys


REQUIRED_PREFIXES = (
    "server_live_world_loaded blocks=",
    "server_live_world_generation available=1",
    "server_live_chunk_generate origin=(0,0) edits=",
    "server_live_module_validation valid=1",
    "server_live_bundled_module valid=1 module=octaryn.basegame",
    "server_live_seed_spawn ",
    "server_live_activate active=1",
    "server_live_tick frame=1",
    "server_live_client_commands_submit requested=0 pending_before=0",
    "server_live_client_commands_submit result=0 pending_after=0",
    "server_live_client_commands_submit requested=1 pending_before=0",
    "server_live_client_command_queue queued=1 pending=1",
    "server_live_client_command_queued index=0 kind=",
    "server_live_client_command_drain applied=1 pending=0",
    "server_live_block_command rejected=0 kind=",
    "server_live_block_command rejected=0 kind=SetBlock request=3 applied=1 changed=1 block=(2,3,4,0)",
    "server_live_block_persistence_dirty edits=",
    "server_live_client_command_rejected index=0 kind=",
    "server_live_snapshot_drain result=0",
)


def validate(log_file):
    if not log_file.exists():
        return [f"{log_file}: missing server live debug probe log"]

    lines = [
        line.strip()
        for line in log_file.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    errors = []
    for prefix in REQUIRED_PREFIXES:
        if not any(line.startswith(prefix) for line in lines):
            errors.append(f"{log_file}: missing expected live debug line prefix {prefix!r}, actual {lines}")

    try:
        submit_index = next(index for index, line in enumerate(lines) if line.startswith("server_live_client_commands_submit requested=1 pending_before=0"))
        queue_index = next(index for index, line in enumerate(lines) if line.startswith("server_live_client_command_queue queued=1 pending=1"))
        block_index = next(index for index, line in enumerate(lines) if line.startswith("server_live_block_command rejected=0 kind="))
        dirty_index = next(index for index, line in enumerate(lines) if line.startswith("server_live_block_persistence_dirty edits="))
        drain_index = next(index for index, line in enumerate(lines) if line.startswith("server_live_client_command_drain applied=1 pending=0"))
        snapshot_index = next(index for index, line in enumerate(lines) if line.startswith("server_live_snapshot_drain result=0"))
    except StopIteration:
        return errors

    expected_order = (
        submit_index,
        queue_index,
        block_index,
        dirty_index,
        drain_index,
        snapshot_index,
    )
    if list(expected_order) != sorted(expected_order):
        errors.append(f"{log_file}: expected submit before queue before block apply before persistence dirty before drain summary before snapshot drain, actual {lines}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-file", required=True)
    args = parser.parse_args()

    errors = validate(pathlib.Path(args.log_file))
    if errors:
        for error in errors:
            print(f"server live debug probe log policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
