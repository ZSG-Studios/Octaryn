from __future__ import annotations

import json

from .constants import (
    ALLOWED_BLOCK_FIELDS,
    ALLOWED_TOP_LEVEL_FIELDS,
    ATLAS_DIRECTIONS,
    EXPECTED_ATLAS,
    EXPECTED_GRASS_BASE,
    EXPECTED_OLD_SOURCE_NAMES,
    EXPECTED_SOLID_BASE,
    REQUIRED_BOOL_FIELDS,
)


def validate_catalog(path):
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
        errors.append(
            f"{path}: expected {len(EXPECTED_OLD_SOURCE_NAMES)} block records, "
            f"found {len(blocks)}")

    ids = set()
    for expected_id, expected_name in enumerate(EXPECTED_OLD_SOURCE_NAMES):
        if expected_id >= len(blocks):
            break
        block = blocks[expected_id]
        validate_block_record(errors, path, ids, expected_id, expected_name, block)

    return errors


def validate_block_record(errors, path, ids, expected_id, expected_name, block):
    block_id = block.get("id")
    expected_id_text = expected_block_id(expected_name)
    if not isinstance(block_id, str) or not block_id.startswith("octaryn.basegame.block."):
        errors.append(f"{path}: block index {expected_id} has invalid id {block_id!r}")
    elif block_id != expected_id_text:
        errors.append(
            f"{path}: block index {expected_id} has id {block_id!r}, "
            f"expected {expected_id_text!r}")
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


def validate_canonical_catalog_file(errors, path):
    if path.name != "octaryn.basegame.blocks.json":
        errors.append(
            f"{path}: canonical block catalog filename must be octaryn.basegame.blocks.json")

    for sibling in sorted(path.parent.glob("octaryn.basegame.block.*.json")):
        errors.append(
            f"{path}: block catalog is canonical; remove duplicate sibling {sibling.name}")


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


def validate_skylight(errors, path, block):
    block_id = block.get("id", "<unknown>")
    value = block.get("skylightOpacity")
    if not isinstance(value, int) or value < 0 or value > 15:
        errors.append(f"{path}: block {block_id} skylightOpacity must be 0 through 15")


def validate_old_source_behavior(errors, path, old_source_index, block):
    block_id = block.get("id", "<unknown>")
    fluid_kind, fluid_level = expected_fluid(old_source_index)

    expected_placeable = (
        old_source_index > 0 and
        old_source_index != 8 and
        old_source_index not in range(15, 22) and
        old_source_index not in range(32, 39))
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
        errors.append(
            f"{path}: block {block_id} requiresSolidBase must be {expected_requires_solid_base}")

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
