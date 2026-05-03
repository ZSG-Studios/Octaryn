#!/usr/bin/env python3
import argparse
import pathlib
import sys


REQUIRED_LINES = (
    "window_show=0",
    "renderer_create=0",
    "initialize=0",
    "presentation_probe_snapshot=0",
    "presentation_updates_drained=1",
    "presented_block_count=1",
    "shutdown=0",
)


def validate(log_file):
    if not log_file.exists():
        return [f"{log_file}: missing client app launch probe log"]

    lines = [
        line.strip()
        for line in log_file.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    if not lines or not lines[0].startswith("crash_marker=/tmp/octaryn-crash-"):
        return [f"{log_file}: missing crash diagnostics marker line, actual {lines}"]

    errors = []
    for expected in REQUIRED_LINES:
        if expected not in lines:
            errors.append(f"{log_file}: missing expected line {expected!r}, actual {lines}")

    tick_count = sum(1 for line in lines if line == "tick=0")
    if tick_count < 2:
        errors.append(f"{log_file}: expected at least two successful ticks, actual {lines}")

    try:
        drain_index = lines.index("presentation_updates_drained=1")
        present_index = lines.index("presented_block_count=1")
        shutdown_index = lines.index("shutdown=0")
    except ValueError:
        return errors

    if not drain_index < present_index < shutdown_index:
        errors.append(
            f"{log_file}: expected drain before presented block before shutdown, actual {lines}"
        )

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-file", required=True)
    args = parser.parse_args()

    errors = validate(pathlib.Path(args.log_file))
    if errors:
        for error in errors:
            print(f"client app launch probe log policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
