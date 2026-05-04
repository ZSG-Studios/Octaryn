#!/usr/bin/env python3
import argparse
import pathlib
import sys


REQUIRED_PREFIXES = (
    "server_live_world_loaded blocks=",
    "server_live_world_generation available=1",
    "server_live_module_validation valid=1",
    "server_live_bundled_module valid=1 module=octaryn.basegame",
    "server_live_player_spawn_align ",
    "server_live_activate active=1",
    "server_live_player_load loaded=",
    "server_live_player_state frame=1 tick_input=1 authority=server",
    "server_live_tick frame=1",
    "server_live_client_command_drain applied=",
    "server_live_chunk_request center=(0,0) radius=0 columns=1 blocks=0",
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
