from __future__ import annotations

from .constants import (
    EXPECTED_ATLAS_LAYER_COUNT,
    EXPECTED_ATLAS_TILE_SIZE,
    PNG_SIGNATURE,
)


def validate_atlas_assets(errors, asset_paths):
    for label, path in asset_paths:
        validate_png_atlas_asset(errors, label, path)


def validate_png_atlas_asset(errors, label, path):
    if path.name != f"basegame-{label}.png":
        errors.append(f"{path}: basegame {label} atlas filename must be basegame-{label}.png")
    if not path.exists():
        errors.append(f"{path}: basegame {label} atlas is missing")
        return

    data = path.read_bytes()
    if len(data) < 33 or data[:8] != PNG_SIGNATURE:
        errors.append(f"{path}: basegame {label} atlas must be a PNG file")
        return

    width = int.from_bytes(data[16:20], "big")
    height = int.from_bytes(data[20:24], "big")
    bit_depth = data[24]
    color_type = data[25]
    expected_width = EXPECTED_ATLAS_TILE_SIZE * EXPECTED_ATLAS_LAYER_COUNT
    if width != expected_width or height != EXPECTED_ATLAS_TILE_SIZE:
        errors.append(
            f"{path}: basegame {label} atlas size must be "
            f"{expected_width}x{EXPECTED_ATLAS_TILE_SIZE}, found {width}x{height}")
    if bit_depth != 8 or color_type != 6:
        errors.append(f"{path}: basegame {label} atlas must be 8-bit RGBA PNG")


def validate_animation_assets(errors, atlas_path, manifest_path):
    if manifest_path.name != "basegame-animation.txt":
        errors.append(f"{manifest_path}: animation manifest filename must be basegame-animation.txt")
    if not manifest_path.exists():
        errors.append(f"{manifest_path}: animation manifest is missing")
        validate_png_animation_atlas(errors, atlas_path, expected_frames=0)
        return

    lines = manifest_path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "Octaryn generated atlas animations":
        errors.append(
            f"{manifest_path}: animation manifest must start with Octaryn generated atlas animations")
    fields = {}
    animation_lines = []
    for line in lines[1:]:
        if line.startswith("animation="):
            animation_lines.append(line)
            continue
        key, separator, value = line.partition("=")
        if separator:
            fields[key] = value

    tile_size = parse_non_negative_int(fields.get("tile_size"))
    frames = parse_non_negative_int(fields.get("frames"))
    animations = parse_non_negative_int(fields.get("animations"))
    if tile_size != EXPECTED_ATLAS_TILE_SIZE:
        errors.append(f"{manifest_path}: animation manifest tile_size must be {EXPECTED_ATLAS_TILE_SIZE}")
    if frames is None:
        errors.append(f"{manifest_path}: animation manifest frames must be a non-negative integer")
        frames = 0
    if animations is None:
        errors.append(f"{manifest_path}: animation manifest animations must be a non-negative integer")
        animations = 0
    if len(animation_lines) != animations:
        errors.append(
            f"{manifest_path}: animation manifest declares {animations} animations "
            f"but contains {len(animation_lines)} animation lines")
    validate_png_animation_atlas(errors, atlas_path, expected_frames=frames)


def parse_non_negative_int(value):
    if value is None:
        return None
    try:
        parsed = int(value)
    except ValueError:
        return None
    return parsed if parsed >= 0 else None


def validate_png_animation_atlas(errors, path, expected_frames):
    if path.name != "basegame-animation.png":
        errors.append(f"{path}: animation atlas filename must be basegame-animation.png")
    if not path.exists():
        errors.append(f"{path}: animation atlas is missing")
        return

    data = path.read_bytes()
    if len(data) < 33 or data[:8] != PNG_SIGNATURE:
        errors.append(f"{path}: animation atlas must be a PNG file")
        return

    width = int.from_bytes(data[16:20], "big")
    height = int.from_bytes(data[20:24], "big")
    bit_depth = data[24]
    color_type = data[25]
    expected_width = max(1, expected_frames) * EXPECTED_ATLAS_TILE_SIZE
    if width != expected_width or height != EXPECTED_ATLAS_TILE_SIZE:
        errors.append(
            f"{path}: animation atlas size must be "
            f"{expected_width}x{EXPECTED_ATLAS_TILE_SIZE}, found {width}x{height}")
    if bit_depth != 8 or color_type != 6:
        errors.append(f"{path}: animation atlas must be 8-bit RGBA PNG")
