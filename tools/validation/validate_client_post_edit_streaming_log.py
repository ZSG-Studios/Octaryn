#!/usr/bin/env python3
import argparse
from pathlib import Path
import re
import sys

from client_app_launch_probe_log.parsing import parse_named_int, read_log_lines


CENTER_PATTERN = re.compile(r" center=\((-?\d+),(-?\d+)\)")


def parse_center(line):
    match = CENTER_PATTERN.search(line)
    if not match:
        return None
    return tuple(int(value) for value in match.groups())


def lines_after_first_edit(lines, prefix):
    for index, line in enumerate(lines):
        if line.startswith(prefix) and "active=0" not in line:
            return lines[index + 1 :]
    return []


def distinct_centers_after_edit(lines):
    centers = [
        center
        for center in (parse_center(line) for line in lines)
        if center is not None
    ]
    return centers, set(centers)


def validate_client_post_edit_streaming(log_file, lines, route, errors):
    after_edit = lines_after_first_edit(lines, "live_block_interaction_intent ")
    if not after_edit:
        errors.append(f"{log_file}: expected a client block edit before post-edit streaming validation")
        return

    center_lines = [
        line
        for line in after_edit
        if line.startswith("live_chunk_view_intent source=process_file ")
    ]
    centers, distinct_centers = distinct_centers_after_edit(center_lines)
    minimum_centers = 4 if route == "straight-after-edits" else 3
    if len(distinct_centers) < minimum_centers:
        errors.append(f"{log_file}: expected chunk-view centers to keep advancing after edits, actual centers={centers[:12]}...{centers[-12:]}")

    if route == "straight-after-edits" and centers:
        x_values = [center[0] for center in centers]
        z_values = [center[1] for center in centers]
        x_delta = max(x_values) - min(x_values)
        z_delta = max(z_values) - min(z_values)
        if max(x_delta, z_delta) < 3:
            errors.append(f"{log_file}: expected straight post-edit movement across chunk centers, actual centers={centers[:12]}...{centers[-12:]}")

    batch_lines = [
        line
        for line in after_edit
        if line.startswith("live_server_stream_mesh_batch ")
        and " active=1 source=server_seed_memory" in line
    ]
    build_batches = [
        line
        for line in batch_lines
        if (parse_named_int(line, "build_columns") or 0) > 0
    ]
    if len(build_batches) < 6:
        errors.append(f"{log_file}: expected post-edit stream mesh batches with build work, actual batches={batch_lines}")


def validate_server_post_edit_streaming(server_log, lines, route, errors):
    after_edit = lines_after_first_edit(lines, "server_live_block_interaction_intent active=1 ")
    if not after_edit:
        errors.append(f"{server_log}: expected a server block edit before post-edit streaming validation")
        return

    window_lines = [line for line in after_edit if line.startswith("server_live_chunk_window ")]
    centers, distinct_centers = distinct_centers_after_edit(window_lines)
    minimum_centers = 4 if route == "straight-after-edits" else 3
    if len(distinct_centers) < minimum_centers:
        errors.append(f"{server_log}: expected server chunk windows to keep advancing after edits, actual centers={centers[:12]}...{centers[-12:]}")

    unloads = [
        value
        for value in (parse_named_int(line, "unload") for line in window_lines)
        if value is not None
    ]
    if max(unloads, default=0) <= 0:
        errors.append(f"{server_log}: expected post-edit chunk unloading while moving, actual unloads={unloads}")


def validate(log_file, server_log, route):
    errors = []
    validate_client_post_edit_streaming(log_file, read_log_lines(log_file), route, errors)
    if server_log is None or not server_log.exists():
        errors.append(f"{server_log}: expected bundled server live log")
    else:
        validate_server_post_edit_streaming(server_log, read_log_lines(server_log), route, errors)
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-file", required=True, type=Path)
    parser.add_argument("--server-log", type=Path)
    parser.add_argument("--route", required=True)
    args = parser.parse_args()
    errors = validate(args.log_file, args.server_log, args.route)
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
