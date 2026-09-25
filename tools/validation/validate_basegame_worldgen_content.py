#!/usr/bin/env python3
import argparse
import json
import pathlib
import sys


# These documents describe compiled revision 3; they are not runtime tuning input.
EXPECTED_FEATURES = {'id': 'octaryn.basegame.features',
 'kind': 'feature',
 'schema': 'octaryn.basegame.features.v2',
 'implementation': 'compiled',
 'generatorRevision': 3,
 'eligibility': {'surface': 'octaryn.basegame.block.grass',
                 'terrainHeightMinExclusive': 30,
                 'terrainHeightMaxInclusive': 240},
 'sampling': 'seeded_coordinate_hash',
 'placementOrder': ['trees', 'bushes', 'flowers'],
 'features': [{'id': 'octaryn.basegame.feature.trees',
               'anchorGridSize': 7,
               'anchorOffsetRangeInclusive': [1, 5],
               'chanceMaxExclusive': {'forest': 0.8, 'plains': 0.22},
               'trunkHeightRangeInclusive': [4, 5],
               'canopyRadius': 1,
               'canopyLayers': 2,
               'neighborCanopies': True,
               'trunk': 'octaryn.basegame.block.log',
               'leaves': 'octaryn.basegame.block.leaves'},
              {'id': 'octaryn.basegame.feature.bushes',
               'chanceMinInclusive': 0.0,
               'chanceMaxExclusive': 0.11,
               'onlyWhenTreeNotPlaced': True,
               'blocks': ['octaryn.basegame.block.bush']},
              {'id': 'octaryn.basegame.feature.flowers',
               'chanceMinInclusive': 0.11,
               'chanceMaxExclusive': 0.14,
               'onlyWhenTreeNotPlaced': True,
               'blocks': ['octaryn.basegame.block.bluebell',
                          'octaryn.basegame.block.gardenia',
                          'octaryn.basegame.block.lavender',
                          'octaryn.basegame.block.rose']}]}
EXPECTED_BIOMES = {'id': 'octaryn.basegame.biomes',
 'kind': 'biome',
 'schema': 'octaryn.basegame.biomes.v2',
 'implementation': 'compiled',
 'generatorRevision': 3,
 'selection': 'first_matching_condition',
 'biomes': [{'id': 'octaryn.basegame.biome.ocean',
             'condition': 'terrain_height < water_height - 2',
             'surface': 'octaryn.basegame.block.sand',
             'fill': 'octaryn.basegame.block.sand',
             'features': []},
            {'id': 'octaryn.basegame.biome.beach',
             'condition': 'terrain_height <= water_height + 2',
             'surface': 'octaryn.basegame.block.sand',
             'fill': 'octaryn.basegame.block.sand',
             'features': []},
            {'id': 'octaryn.basegame.biome.alpine',
             'condition': 'temperature - max(0, terrain_height - 60) * 0.007 < -0.38 or terrain_height > 150',
             'surface': 'octaryn.basegame.block.snow',
             'fill': 'octaryn.basegame.block.stone',
             'features': []},
            {'id': 'octaryn.basegame.biome.desert',
             'condition': 'temperature > 0.18 and humidity < -0.1',
             'surface': 'octaryn.basegame.block.sand',
             'fill': 'octaryn.basegame.block.sand',
             'features': []},
            {'id': 'octaryn.basegame.biome.forest',
             'condition': 'humidity > 0.08',
             'surface': 'octaryn.basegame.block.grass',
             'fill': 'octaryn.basegame.block.dirt',
             'features': ['octaryn.basegame.feature.trees',
                          'octaryn.basegame.feature.bushes',
                          'octaryn.basegame.feature.flowers'],
             'highElevationOverride': {'terrainHeightMinExclusive': 105,
                                       'surface': 'octaryn.basegame.block.stone',
                                       'fill': 'octaryn.basegame.block.stone',
                                       'features': []}},
            {'id': 'octaryn.basegame.biome.plains',
             'condition': 'otherwise',
             'surface': 'octaryn.basegame.block.grass',
             'fill': 'octaryn.basegame.block.dirt',
             'features': ['octaryn.basegame.feature.trees',
                          'octaryn.basegame.feature.bushes',
                          'octaryn.basegame.feature.flowers'],
             'highElevationOverride': {'terrainHeightMinExclusive': 105,
                                       'surface': 'octaryn.basegame.block.stone',
                                       'fill': 'octaryn.basegame.block.stone',
                                       'features': []}}]}
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
    "vegetation",
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


def validate_compiled_descriptor(errors, path, document, expected):
    for field in sorted(set(document) | set(expected)):
        if field not in expected:
            errors.append(f"{path}: compiled descriptor has unknown field {field!r}")
        elif type(document.get(field)) is not type(expected[field]) or document.get(field) != expected[field]:
            errors.append(f"{path}: compiled descriptor {field} must match generator revision 3")


def collect_feature_ids(errors, path, block_ids):
    document = load_json(path)
    validate_compiled_descriptor(errors, path, document, EXPECTED_FEATURES)
    features = document.get("features")
    if not isinstance(features, list):
        return set()
    feature_ids = set()
    for feature in features:
        if not isinstance(feature, dict):
            continue
        feature_id = feature.get("id")
        if not isinstance(feature_id, str):
            continue
        feature_ids.add(feature_id)
        for field in ("trunk", "leaves"):
            if field in feature:
                validate_block_reference(errors, path, feature_id, field, feature[field], block_ids)
        for block_id in feature.get("blocks", []) if isinstance(feature.get("blocks", []), list) else []:
            validate_block_reference(errors, path, feature_id, "blocks", block_id, block_ids)
    return feature_ids


def validate_biomes(errors, path, block_ids, feature_ids):
    document = load_json(path)
    validate_compiled_descriptor(errors, path, document, EXPECTED_BIOMES)
    biomes = document.get("biomes")
    if not isinstance(biomes, list):
        return
    for biome in biomes:
        if not isinstance(biome, dict):
            continue
        biome_id = biome.get("id")
        for field in ("surface", "fill"):
            validate_block_reference(errors, path, biome_id, field, biome.get(field), block_ids)
        for feature_id in biome.get("features", []) if isinstance(biome.get("features", []), list) else []:
            if not isinstance(feature_id, str) or feature_id not in feature_ids:
                errors.append(f"{path}: biome {biome_id!r} references unknown feature id {feature_id!r}")


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
        "generatorRevision": 3,
        "seed": 1337,
        "waterHeight": 30,
        "waterBlock": "octaryn.basegame.block.water",
        "landforms": ["domain_warp", "continentalness", "erosion", "ridges", "rivers"],
        "climate": ["temperature", "humidity"],
        "caves": ["protected_roof", "chambers", "tunnels"],
        "undergroundFluids": "air",
        "vegetation": ["world_aligned_trees", "grass_bushes", "flowers", "neighbor_canopies"],
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
