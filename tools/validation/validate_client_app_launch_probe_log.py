#!/usr/bin/env python3
import argparse
import pathlib
import sys


REQUIRED_LINES = (
    "window_show=0",
    "renderer_create=0",
    "basegame_atlas_manifest=loaded",
    "initialize=0",
    "world_blocks_snapshot=0",
    "shutdown=0",
)


def parse_positive_count(lines, prefix):
    values = []
    for line in lines:
        if not line.startswith(prefix):
            continue
        try:
            values.append(int(line.removeprefix(prefix)))
        except ValueError:
            return []
    return values


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

    loaded_blocks = parse_positive_count(lines, "world_blocks_loaded=")
    if not loaded_blocks or max(loaded_blocks) <= 1:
        errors.append(f"{log_file}: expected generated world block load, actual {lines}")

    surface_blocks = parse_positive_count(lines, "world_surface_blocks_applied=")
    if not surface_blocks or max(surface_blocks) <= 1:
        errors.append(f"{log_file}: expected generated world surface block snapshot, actual {lines}")

    atlas_layers = parse_positive_count(lines, "basegame_atlas_layers=")
    if not atlas_layers or max(atlas_layers) != 29:
        errors.append(f"{log_file}: expected 29 basegame atlas layers, actual {lines}")

    atlas_tile_size = parse_positive_count(lines, "basegame_atlas_tile_size=")
    if not atlas_tile_size or max(atlas_tile_size) != 32:
        errors.append(f"{log_file}: expected 32 px basegame atlas tiles, actual {lines}")

    drained_updates = parse_positive_count(lines, "presentation_updates_drained=")
    if not drained_updates or sum(drained_updates) <= 1:
        errors.append(f"{log_file}: expected multiple presentation updates drained, actual {lines}")

    presented_blocks = parse_positive_count(lines, "presented_block_count=")
    if not presented_blocks or max(presented_blocks) <= 1:
        errors.append(f"{log_file}: expected multiple presented blocks, actual {lines}")

    clear_pixels = parse_positive_count(lines, "rendered_clear_pixels=")
    if not clear_pixels or max(clear_pixels) <= 0:
        errors.append(f"{log_file}: expected visible clear pixels, actual {lines}")

    block_pixels = parse_positive_count(lines, "rendered_block_pixels=")
    if not block_pixels or max(block_pixels) <= 0:
        errors.append(f"{log_file}: expected visible block pixels, actual {lines}")

    try:
        atlas_index = lines.index("basegame_atlas_manifest=loaded")
        initialize_index = lines.index("initialize=0")
        snapshot_index = lines.index("world_blocks_snapshot=0")
        drain_index = next(index for index, line in enumerate(lines) if line.startswith("presentation_updates_drained="))
        present_index = next(index for index, line in enumerate(lines) if line.startswith("presented_block_count="))
        shutdown_index = lines.index("shutdown=0")
    except (StopIteration, ValueError):
        return errors

    if not atlas_index < initialize_index < snapshot_index < drain_index < present_index < shutdown_index:
        errors.append(
            f"{log_file}: expected atlas load before initialize before world snapshot before drain before presented blocks before shutdown, actual {lines}"
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
