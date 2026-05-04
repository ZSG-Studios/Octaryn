#!/usr/bin/env python3
import argparse
import pathlib
import sys


EXPECTED_LINES = {
    "client": (
        "tick_before_initialize=-1",
        "apply_server_snapshot_before_initialize=-1",
        "drain_presentation_updates_before_initialize=-1",
        "initialize=0",
        "tick=0",
        "apply_server_snapshot=0",
        "drain_presentation_updates=0 count=1 x=-4 y=5 z=6 block=7",
        "drain_presentation_updates_empty=0 count=0",
        "apply_server_snapshot_invalid=-2",
        "reinitialize=0",
        "tick_after_reinitialize=0",
        "shutdown=0",
    ),
    "server": (
        "tick_before_initialize=-1",
        "initialize=0",
        "tick=0",
        "reinitialize=0",
        "tick_after_reinitialize=0",
        "request_chunk_columns=0",
        "request_chunk_columns_columns=1",
        "request_chunk_columns_blocks=",
        "submit_client_commands=0",
        "submit_client_commands_set_block_array=0",
        "tick_after_submit=0",
        "submit_client_commands_invalid=-1",
        "drain_server_snapshots=0",
        "drain_server_snapshots_block_changes=1",
        "drain_server_snapshots_empty=0",
        "submit_client_commands_break_block_array=0",
        "tick_after_break_submit=0",
        "drain_server_snapshots_after_break=0",
        "drain_server_snapshots_break_changes=1",
        "shutdown=0",
    ),
}


def validate(owner, log_file):
    if not log_file.exists():
        return [f"{log_file}: missing owner launch probe log"]

    actual = [line.strip() for line in log_file.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not actual or not actual[0].startswith("crash_marker=/tmp/octaryn-crash-"):
        return [f"{log_file}: missing crash diagnostics marker line, actual {actual}"]

    expected = [actual[0], *EXPECTED_LINES[owner]]
    if len(actual) != len(expected):
        return [f"{log_file}: expected {expected}, actual {actual}"]

    for index, (actual_line, expected_line) in enumerate(zip(actual, expected)):
        if expected_line == "request_chunk_columns_blocks=":
            if not actual_line.startswith(expected_line):
                return [f"{log_file}: expected {expected}, actual {actual}"]
            try:
                block_count = int(actual_line.removeprefix(expected_line))
            except ValueError:
                return [f"{log_file}: invalid chunk block count line {actual_line!r}"]
            if block_count <= 1024:
                return [f"{log_file}: expected streamed chunk blocks, actual {actual_line!r}"]
            continue

        if actual_line != expected_line:
            return [f"{log_file}: mismatch at line {index}: expected {expected}, actual {actual}"]

    return []


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--owner", choices=sorted(EXPECTED_LINES), required=True)
    parser.add_argument("--log-file", required=True)
    args = parser.parse_args()

    errors = validate(args.owner, pathlib.Path(args.log_file))
    if errors:
        for error in errors:
            print(f"owner launch probe log policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
