#!/usr/bin/env python3
import argparse
import json
import pathlib
import subprocess
import sys


EXPECTED_OLD_SOURCE_NAMES = (
    "BLOCK_EMPTY",
    "BLOCK_GRASS",
    "BLOCK_DIRT",
    "BLOCK_SAND",
    "BLOCK_SNOW",
    "BLOCK_STONE",
    "BLOCK_LOG",
    "BLOCK_LEAVES",
    "BLOCK_CLOUD",
    "BLOCK_BUSH",
    "BLOCK_BLUEBELL",
    "BLOCK_GARDENIA",
    "BLOCK_ROSE",
    "BLOCK_LAVENDER",
    "BLOCK_WATER",
    "BLOCK_WATER_1",
    "BLOCK_WATER_2",
    "BLOCK_WATER_3",
    "BLOCK_WATER_4",
    "BLOCK_WATER_5",
    "BLOCK_WATER_6",
    "BLOCK_WATER_7",
    "BLOCK_RED_TORCH",
    "BLOCK_GREEN_TORCH",
    "BLOCK_BLUE_TORCH",
    "BLOCK_YELLOW_TORCH",
    "BLOCK_CYAN_TORCH",
    "BLOCK_MAGENTA_TORCH",
    "BLOCK_WHITE_TORCH",
    "BLOCK_PLANKS",
    "BLOCK_GLASS",
    "BLOCK_LAVA",
    "BLOCK_LAVA_1",
    "BLOCK_LAVA_2",
    "BLOCK_LAVA_3",
    "BLOCK_LAVA_4",
    "BLOCK_LAVA_5",
    "BLOCK_LAVA_6",
    "BLOCK_LAVA_7",
)

REQUIRED_BOOL_FIELDS = (
    "opaque",
    "sprite",
    "solid",
    "occlusion",
    "placeable",
    "targetable",
    "requiresGrass",
    "requiresSolidBase",
    "fluidSource",
)

ATLAS_DIRECTIONS = ("north", "south", "east", "west", "up", "down")
EXPECTED_ATLAS_TILE_SIZE = 32
EXPECTED_ATLAS_LAYER_COUNT = 29
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

ALLOWED_TOP_LEVEL_FIELDS = {"id", "kind", "schema", "blocks"}

ALLOWED_BLOCK_FIELDS = {
    "id",
    "displayName",
    "opaque",
    "sprite",
    "solid",
    "occlusion",
    "placeable",
    "targetable",
    "requiresGrass",
    "requiresSolidBase",
    "skylightOpacity",
    "fluidKind",
    "fluidLevel",
    "fluidSource",
    "atlas",
}

EXPECTED_GRASS_BASE = {9, 10, 11, 12, 13}
EXPECTED_SOLID_BASE = {22, 23, 24, 25, 26, 27, 28}

EXPECTED_ATLAS = {
    0: (0, 0, 0, 0, 0, 0),
    1: (2, 2, 2, 2, 1, 3),
    2: (3, 3, 3, 3, 3, 3),
    3: (5, 5, 5, 5, 5, 5),
    4: (6, 6, 6, 6, 6, 6),
    5: (4, 4, 4, 4, 4, 4),
    6: (8, 8, 8, 8, 7, 7),
    7: (10, 10, 10, 10, 10, 10),
    8: (9, 9, 9, 9, 9, 9),
    9: (15, 15, 15, 15, 15, 15),
    10: (13, 13, 13, 13, 13, 13),
    11: (12, 12, 12, 12, 12, 12),
    12: (11, 11, 11, 11, 11, 11),
    13: (14, 14, 14, 14, 14, 14),
    14: (16, 16, 16, 16, 16, 16),
    15: (16, 16, 16, 16, 16, 16),
    16: (16, 16, 16, 16, 16, 16),
    17: (16, 16, 16, 16, 16, 16),
    18: (16, 16, 16, 16, 16, 16),
    19: (16, 16, 16, 16, 16, 16),
    20: (16, 16, 16, 16, 16, 16),
    21: (16, 16, 16, 16, 16, 16),
    22: (17, 17, 17, 17, 17, 17),
    23: (18, 18, 18, 18, 18, 18),
    24: (19, 19, 19, 19, 19, 19),
    25: (20, 20, 20, 20, 20, 20),
    26: (21, 21, 21, 21, 21, 21),
    27: (22, 22, 22, 22, 22, 22),
    28: (23, 23, 23, 23, 23, 23),
    29: (24, 24, 24, 24, 24, 24),
    30: (25, 25, 25, 25, 25, 25),
    31: (27, 27, 27, 27, 27, 27),
    32: (27, 27, 27, 27, 27, 27),
    33: (27, 27, 27, 27, 27, 27),
    34: (27, 27, 27, 27, 27, 27),
    35: (27, 27, 27, 27, 27, 27),
    36: (27, 27, 27, 27, 27, 27),
    37: (27, 27, 27, 27, 27, 27),
    38: (27, 27, 27, 27, 27, 27),
}


def validate(path):
    errors = []
    catalog = json.loads(path.read_text(encoding="utf-8"))
    validate_canonical_catalog_file(errors, path)
    validate_top_level_fields(errors, path, catalog)
    if catalog.get("id") != "octaryn.basegame.blocks":
        errors.append(f"{path}: id must be octaryn.basegame.blocks")
    if catalog.get("kind") != "block":
        errors.append(f"{path}: kind must be block")
    if catalog.get("schema") != "octaryn.basegame.blocks.v1":
        errors.append(f"{path}: schema must be octaryn.basegame.blocks.v1")
    blocks = catalog.get("blocks")
    if not isinstance(blocks, list):
        return errors + [f"{path}: blocks must be a list"]

    if len(blocks) != len(EXPECTED_OLD_SOURCE_NAMES):
        errors.append(f"{path}: expected {len(EXPECTED_OLD_SOURCE_NAMES)} block records, found {len(blocks)}")

    ids = set()
    for expected_id, expected_name in enumerate(EXPECTED_OLD_SOURCE_NAMES):
        if expected_id >= len(blocks):
            break
        block = blocks[expected_id]

        block_id = block.get("id")
        if not isinstance(block_id, str) or not block_id.startswith("octaryn.basegame.block."):
            errors.append(f"{path}: block index {expected_id} has invalid id {block_id!r}")
        elif block_id != expected_block_id(expected_name):
            errors.append(f"{path}: block index {expected_id} has id {block_id!r}, expected {expected_block_id(expected_name)!r}")
        elif block_id in ids:
            errors.append(f"{path}: duplicate block id {block_id}")
        ids.add(block_id)

        if not isinstance(block.get("displayName"), str) or not block["displayName"].strip():
            errors.append(f"{path}: block {block_id} has missing displayName")

        validate_block_fields(errors, path, block)
        for field in REQUIRED_BOOL_FIELDS:
            if not isinstance(block.get(field), bool):
                errors.append(f"{path}: block {block_id} field {field} must be boolean")

        validate_fluid(errors, path, block)
        validate_atlas(errors, path, block)
        validate_skylight(errors, path, block)
        validate_old_source_behavior(errors, path, expected_id, block)

    return errors


def validate_generated_source(errors, catalog_path, source_path):
    if source_path.name != "BasegameBlockCatalog.cs":
        errors.append(f"{source_path}: generated block catalog source must be BasegameBlockCatalog.cs")
        return
    if not source_path.exists():
        errors.append(f"{source_path}: generated block catalog source is missing")
        return

    generator_path = catalog_path.parents[2] / "Tools" / "generate_block_catalog_source.py"
    result = subprocess.run(
        [sys.executable, str(generator_path), "--catalog", str(catalog_path), "--output", "-"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True)
    if result.returncode != 0:
        errors.append(f"{generator_path}: failed to render generated source: {result.stderr.strip()}")
        return

    actual = source_path.read_text(encoding="utf-8")
    if actual != result.stdout:
        errors.append(f"{source_path}: generated block catalog source does not match {catalog_path}")


def validate_canonical_catalog_file(errors, path):
    if path.name != "octaryn.basegame.blocks.json":
        errors.append(f"{path}: canonical block catalog filename must be octaryn.basegame.blocks.json")

    for sibling in sorted(path.parent.glob("octaryn.basegame.block.*.json")):
        errors.append(f"{path}: block catalog is canonical; remove duplicate sibling {sibling.name}")


def validate_top_level_fields(errors, path, catalog):
    unknown = sorted(set(catalog) - ALLOWED_TOP_LEVEL_FIELDS)
    for field in unknown:
        errors.append(f"{path}: unknown top-level field {field!r}")


def validate_block_fields(errors, path, block):
    block_id = block.get("id", "<unknown>")
    unknown = sorted(set(block) - ALLOWED_BLOCK_FIELDS)
    for field in unknown:
        errors.append(f"{path}: block {block_id} has unknown field {field!r}")


def validate_fluid(errors, path, block):
    block_id = block.get("id", "<unknown>")
    fluid_kind = block.get("fluidKind")
    fluid_level = block.get("fluidLevel")
    if fluid_kind not in {"none", "water", "lava"}:
        errors.append(f"{path}: block {block_id} has invalid fluidKind {fluid_kind!r}")
        return
    if not isinstance(fluid_level, int):
        errors.append(f"{path}: block {block_id} has non-integer fluidLevel")
        return
    if fluid_kind == "none" and fluid_level != -1:
        errors.append(f"{path}: block {block_id} non-fluid must use fluidLevel -1")
    if fluid_kind != "none" and fluid_level not in range(0, 8):
        errors.append(f"{path}: block {block_id} fluid level must be 0 through 7")
    if block.get("fluidSource") is True and fluid_level != 0:
        errors.append(f"{path}: block {block_id} source fluid must use level 0")


def validate_atlas(errors, path, block):
    block_id = block.get("id", "<unknown>")
    atlas = block.get("atlas")
    if not isinstance(atlas, dict):
        errors.append(f"{path}: block {block_id} atlas must be an object")
        return
    unknown = sorted(set(atlas) - set(ATLAS_DIRECTIONS))
    for direction in unknown:
        errors.append(f"{path}: block {block_id} atlas has unknown direction {direction!r}")
    for direction in ATLAS_DIRECTIONS:
        value = atlas.get(direction)
        if not isinstance(value, int) or value < 0 or value >= 32:
            errors.append(f"{path}: block {block_id} atlas.{direction} must be 0 through 31")
    block_index = block_index_from_id(block_id)
    expected = EXPECTED_ATLAS.get(block_index)
    actual = tuple(atlas.get(direction) for direction in ATLAS_DIRECTIONS)
    if expected is not None and actual != expected:
        errors.append(f"{path}: block {block_id} atlas {actual!r} must match old source {expected!r}")


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
        errors.append(f"{manifest_path}: animation manifest must start with Octaryn generated atlas animations")
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


def validate_skylight(errors, path, block):
    block_id = block.get("id", "<unknown>")
    value = block.get("skylightOpacity")
    if not isinstance(value, int) or value < 0 or value > 15:
        errors.append(f"{path}: block {block_id} skylightOpacity must be 0 through 15")


def validate_old_source_behavior(errors, path, old_source_index, block):
    block_id = block.get("id", "<unknown>")
    fluid_kind, fluid_level = expected_fluid(old_source_index)

    expected_placeable = old_source_index > 0 and old_source_index != 8 and old_source_index not in range(15, 22) and old_source_index not in range(32, 39)
    if block.get("placeable") != expected_placeable:
        errors.append(f"{path}: block {block_id} placeable must be {expected_placeable}")

    expected_targetable = old_source_index != 0 and (
        block.get("solid") is True or
        block.get("sprite") is True or
        block.get("requiresGrass") is True or
        block.get("requiresSolidBase") is True)
    if block.get("targetable") != expected_targetable:
        errors.append(f"{path}: block {block_id} targetable must be {expected_targetable}")

    expected_requires_grass = old_source_index in EXPECTED_GRASS_BASE
    if block.get("requiresGrass") != expected_requires_grass:
        errors.append(f"{path}: block {block_id} requiresGrass must be {expected_requires_grass}")

    expected_requires_solid_base = old_source_index in EXPECTED_SOLID_BASE
    if block.get("requiresSolidBase") != expected_requires_solid_base:
        errors.append(f"{path}: block {block_id} requiresSolidBase must be {expected_requires_solid_base}")

    if block.get("fluidKind") != fluid_kind:
        errors.append(f"{path}: block {block_id} fluidKind must be {fluid_kind}")
    if block.get("fluidLevel") != fluid_level:
        errors.append(f"{path}: block {block_id} fluidLevel must be {fluid_level}")
    expected_source = fluid_kind != "none" and fluid_level == 0
    if block.get("fluidSource") != expected_source:
        errors.append(f"{path}: block {block_id} fluidSource must be {expected_source}")

    expected_opacity = expected_skylight_opacity(old_source_index, block)
    if block.get("skylightOpacity") != expected_opacity:
        errors.append(f"{path}: block {block_id} skylightOpacity must be {expected_opacity}")


def expected_fluid(old_source_index):
    if old_source_index in range(14, 22):
        return "water", old_source_index - 14
    if old_source_index in range(31, 39):
        return "lava", old_source_index - 31
    return "none", -1


def expected_skylight_opacity(old_source_index, block):
    if old_source_index in {0, 8, 9, 10, 11, 12, 13, 30}:
        return 0
    if old_source_index == 7:
        return 1
    if old_source_index in range(14, 22) or old_source_index in range(31, 39):
        return 2
    return 15 if block.get("occlusion") is True else 0


def expected_block_id(old_source_name):
    token = old_source_name.removeprefix("BLOCK_").lower()
    if token == "empty":
        token = "air"
    return f"octaryn.basegame.block.{token}"


def block_index_from_id(block_id):
    if not isinstance(block_id, str):
        return None
    try:
        return [expected_block_id(name) for name in EXPECTED_OLD_SOURCE_NAMES].index(block_id)
    except ValueError:
        return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", required=True)
    parser.add_argument("--generated-source")
    parser.add_argument("--atlas-color")
    parser.add_argument("--atlas-normal")
    parser.add_argument("--atlas-specular")
    parser.add_argument("--animation-atlas")
    parser.add_argument("--animation-manifest")
    args = parser.parse_args()

    catalog_path = pathlib.Path(args.catalog)
    errors = validate(catalog_path)
    if args.generated_source:
        validate_generated_source(errors, catalog_path, pathlib.Path(args.generated_source))
    if args.atlas_color or args.atlas_normal or args.atlas_specular:
        required_atlases = (
            ("color", args.atlas_color),
            ("normal", args.atlas_normal),
            ("specular", args.atlas_specular),
        )
        for label, value in required_atlases:
            if not value:
                errors.append(f"--atlas-{label} is required when validating atlas assets")
        if all(value for _label, value in required_atlases):
            validate_atlas_assets(
                errors,
                [(label, pathlib.Path(value)) for label, value in required_atlases])
    if args.animation_atlas or args.animation_manifest:
        if not args.animation_atlas:
            errors.append("--animation-atlas is required when validating animation assets")
        if not args.animation_manifest:
            errors.append("--animation-manifest is required when validating animation assets")
        if args.animation_atlas and args.animation_manifest:
            validate_animation_assets(
                errors,
                pathlib.Path(args.animation_atlas),
                pathlib.Path(args.animation_manifest))
    if errors:
        for error in errors:
            print(f"basegame block catalog policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
