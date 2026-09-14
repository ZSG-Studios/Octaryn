#!/usr/bin/env python3
import argparse
import json
import pathlib
import sys


BIOME_DOCUMENT_FIELDS = {"id", "kind", "schema", "biomes"}
BIOME_FIELDS = {"id", "surface", "features"}
FEATURE_DOCUMENT_FIELDS = {"id", "kind", "schema", "features"}
FEATURE_FIELDS = {"id", "noiseThreshold", "blocks", "trunk", "leaves"}
TERRAIN_RULE_FIELDS = {
    "id",
    "kind",
    "schema",
    "waterHeight",
    "waterBlock",
    "implementation",
    "generatorRevision",
    "seed",
    "landforms",
    "climate",
    "caves",
    "undergroundFluids",
}


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def validate(block_catalog_path, biomes_path, features_path, terrain_rule_path):
    errors = []
    block_ids = collect_block_ids(errors, block_catalog_path)
    feature_ids = collect_feature_ids(errors, features_path, block_ids)
    validate_biomes(errors, biomes_path, block_ids, feature_ids)
    validate_terrain_rule(errors, terrain_rule_path, block_ids)
    return errors


def collect_block_ids(errors, path):
    catalog = load_json(path)
    blocks = catalog.get("blocks")
    if not isinstance(blocks, list):
        errors.append(f"{path}: blocks must be a list")
        return set()

    block_ids = set()
    for index, block in enumerate(blocks):
        block_id = block.get("id") if isinstance(block, dict) else None
        if not isinstance(block_id, str) or not block_id.startswith("octaryn.basegame.block."):
            errors.append(f"{path}: block index {index} has invalid stable block id {block_id!r}")
            continue
        if block_id in block_ids:
            errors.append(f"{path}: duplicate stable block id {block_id}")
        block_ids.add(block_id)
    return block_ids


def collect_feature_ids(errors, path, block_ids):
    document = load_json(path)
    validate_document_identity(errors, path, document, "octaryn.basegame.features", "feature")
    unknown_document_fields = sorted(set(document) - FEATURE_DOCUMENT_FIELDS)
    for field in unknown_document_fields:
        errors.append(f"{path}: features document has unknown field {field!r}")
    if document.get("schema") != "octaryn.basegame.features.v1":
        errors.append(f"{path}: schema must be octaryn.basegame.features.v1")
    features = document.get("features")
    if not isinstance(features, list):
        errors.append(f"{path}: features must be a list")
        return set()

    feature_ids = set()
    for index, feature in enumerate(features):
        if not isinstance(feature, dict):
            errors.append(f"{path}: feature index {index} must be an object")
            continue

        unknown = sorted(set(feature) - FEATURE_FIELDS)
        for field in unknown:
            errors.append(f"{path}: feature index {index} has unknown field {field!r}")

        feature_id = feature.get("id")
        if not isinstance(feature_id, str) or not feature_id.startswith("octaryn.basegame.feature."):
            errors.append(f"{path}: feature index {index} has invalid stable feature id {feature_id!r}")
        elif feature_id in feature_ids:
            errors.append(f"{path}: duplicate stable feature id {feature_id}")
        else:
            feature_ids.add(feature_id)

        threshold = feature.get("noiseThreshold")
        if not is_number(threshold) or threshold < 0.0 or threshold > 1.0:
            errors.append(f"{path}: feature {feature_id!r} noiseThreshold must be 0.0 through 1.0")

        blocks = feature.get("blocks")
        if blocks is not None:
            if not isinstance(blocks, list) or not blocks:
                errors.append(f"{path}: feature {feature_id!r} blocks must be a non-empty list")
            else:
                for block_id in blocks:
                    validate_block_reference(errors, path, feature_id, "blocks", block_id, block_ids)

        for field in ("trunk", "leaves"):
            if field in feature:
                validate_block_reference(errors, path, feature_id, field, feature[field], block_ids)

    return feature_ids


def validate_biomes(errors, path, block_ids, feature_ids):
    document = load_json(path)
    validate_document_identity(errors, path, document, "octaryn.basegame.biomes", "biome")
    unknown_document_fields = sorted(set(document) - BIOME_DOCUMENT_FIELDS)
    for field in unknown_document_fields:
        errors.append(f"{path}: biomes document has unknown field {field!r}")
    if document.get("schema") != "octaryn.basegame.biomes.v1":
        errors.append(f"{path}: schema must be octaryn.basegame.biomes.v1")
    biomes = document.get("biomes")
    if not isinstance(biomes, list):
        errors.append(f"{path}: biomes must be a list")
        return

    biome_ids = set()
    for index, biome in enumerate(biomes):
        if not isinstance(biome, dict):
            errors.append(f"{path}: biome index {index} must be an object")
            continue

        unknown = sorted(set(biome) - BIOME_FIELDS)
        for field in unknown:
            errors.append(f"{path}: biome index {index} has unknown field {field!r}")

        biome_id = biome.get("id")
        if not isinstance(biome_id, str) or not biome_id.startswith("octaryn.basegame.biome."):
            errors.append(f"{path}: biome index {index} has invalid stable biome id {biome_id!r}")
        elif biome_id in biome_ids:
            errors.append(f"{path}: duplicate stable biome id {biome_id}")
        else:
            biome_ids.add(biome_id)

        validate_block_reference(errors, path, biome_id, "surface", biome.get("surface"), block_ids)

        features = biome.get("features")
        if not isinstance(features, list):
            errors.append(f"{path}: biome {biome_id!r} features must be a list")
            continue
        for feature_id in features:
            if not isinstance(feature_id, str) or not feature_id.startswith("octaryn.basegame.feature."):
                errors.append(f"{path}: biome {biome_id!r} has invalid stable feature id {feature_id!r}")
            elif feature_id not in feature_ids:
                errors.append(f"{path}: biome {biome_id!r} references unknown feature id {feature_id}")


def validate_terrain_rule(errors, path, block_ids):
    document = load_json(path)
    validate_document_identity(errors, path, document, "octaryn.basegame.rule.terrain_generation", "rule")
    unknown = sorted(set(document) - TERRAIN_RULE_FIELDS)
    for field in unknown:
        errors.append(f"{path}: terrain generation rule has unknown field {field!r}")
    missing = sorted(TERRAIN_RULE_FIELDS - set(document))
    for field in missing:
        errors.append(f"{path}: terrain generation rule is missing field {field!r}")

    if document.get("schema") != "octaryn.basegame.terrain_generation_rule.v2":
        errors.append(f"{path}: schema must be octaryn.basegame.terrain_generation_rule.v2")
    validate_block_reference(errors, path, "terrain_generation", "waterBlock", document.get("waterBlock"), block_ids)
    # This content describes the compiled sampler; it is not runtime tuning input.
    expected = {
        "implementation": "compiled",
        "generatorRevision": 2,
        "seed": 1337,
        "waterHeight": 30,
        "waterBlock": "octaryn.basegame.block.water",
        "landforms": ["domain_warp", "continentalness", "erosion", "ridges", "rivers"],
        "climate": ["temperature", "humidity"],
        "caves": ["protected_roof", "chambers", "tunnels"],
        "undergroundFluids": "air",
    }
    for field, expected_value in expected.items():
        value = document.get(field)
        if type(value) is not type(expected_value) or value != expected_value:
            errors.append(f"{path}: compiled terrain descriptor {field} must be {expected_value!r}")


def validate_document_identity(errors, path, document, expected_id, expected_kind):
    if document.get("id") != expected_id:
        errors.append(f"{path}: id must be {expected_id}")
    if document.get("kind") != expected_kind:
        errors.append(f"{path}: kind must be {expected_kind}")


def validate_block_reference(errors, path, owner_id, field, block_id, block_ids):
    if not isinstance(block_id, str) or not block_id.startswith("octaryn.basegame.block."):
        errors.append(f"{path}: {owner_id!r} field {field} must use a stable block id, got {block_id!r}")
    elif block_id not in block_ids:
        errors.append(f"{path}: {owner_id!r} field {field} references unknown block id {block_id}")


def is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--block-catalog", required=True)
    parser.add_argument("--biomes", required=True)
    parser.add_argument("--features", required=True)
    parser.add_argument("--terrain-rule", required=True)
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.block_catalog),
        pathlib.Path(args.biomes),
        pathlib.Path(args.features),
        pathlib.Path(args.terrain_rule))
    if errors:
        for error in errors:
            print(f"basegame worldgen content policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
