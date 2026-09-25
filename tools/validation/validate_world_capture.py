#!/usr/bin/env python3
"""Validate retained GPU face captures written by WorldCapture.cpp.

Records: signed column X/Z, uint32 face count, then count {int32 x,y,z; uint32
material | direction<<16 | (width-1)<<20 | (height-1)<<25}. Width/height are 1..32;
only occluding opaque-pass cube faces may merge. Face-buffer ranges are opaque, sprite, glass, water,
lava, independently of forward draw order. This validates GPU geometry metadata;
it does not prove texture pixels, fluid shapes, frame timing or server authority.
"""
import argparse
import collections
import json
import struct
import sys
from pathlib import Path

WIDTH = 32
MIN_Y, MAX_Y = -256, 256
MAX_COLUMN_FACES = WIDTH * WIDTH * (MAX_Y - MIN_Y) * 6
MAX_COLUMNS = 65 * 65
READ_BATCH_FACES = 4096
ERROR_LIMIT = 20
ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = ROOT / "octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json"


def load_catalog(path):
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    blocks = data.get("blocks")
    if data.get("schema") != "octaryn.basegame.blocks.v1" or not isinstance(blocks, list):
        raise ValueError("invalid basegame block catalog schema")
    if not 1 <= len(blocks) <= 65536:
        raise ValueError("catalog block count must be 1..65536")
    for block in blocks:
        if not isinstance(block, dict) or not isinstance(block.get("id"), str):
            raise ValueError("catalog block requires an id")
        for field in ("opaque", "sprite", "requiresSolidBase", "occlusion"):
            if not isinstance(block.get(field), bool):
                raise ValueError(f"catalog block {block['id']} requires boolean {field}")
        if block.get("fluidKind") not in ("none", "water", "lava"):
            raise ValueError(f"invalid fluid kind for {block['id']}")
    return blocks


def mesh_class(block):
    if block["sprite"]:
        return 1
    if block["opaque"]:
        return 0
    if block["fluidKind"] != "none":
        return 4 if block["fluidKind"] == "lava" else 3
    return 2


def load_fixture(path, blocks):
    if path is None:
        return {}
    entries = json.loads(path.read_text(encoding="utf-8-sig")).get("blocks")
    if not isinstance(entries, list) or len(entries) > 4096:
        raise ValueError("fixture requires at most 4096 expected blocks")
    result = {}
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("invalid fixture entry")
        for name in ("x", "y", "z", "block", "expectedFaces"):
            if type(entry.get(name)) is not int:
                raise ValueError(f"fixture requires integer {name}")
        if not 0 < entry["block"] < len(blocks) or not 0 <= entry["expectedFaces"] <= 6:
            raise ValueError("invalid fixture block or face count")
        key = (entry["x"], entry["y"], entry["z"], entry["block"])
        if key in result:
            raise ValueError("duplicate fixture block")
        result[key] = entry
    return result


def audit(capture, blocks, fixture):
    classes = [mesh_class(block) for block in blocks]
    errors = []
    error_count = 0
    counts = collections.Counter()
    pass_directions = collections.Counter()
    actual_fixture = collections.defaultdict(list)
    columns = set()
    total = 0
    unit_total = 0

    def fail(message, **context):
        nonlocal error_count
        error_count += 1
        if len(errors) < ERROR_LIMIT:
            errors.append({"message": message, **context})

    with capture.open("rb") as source:
        file_size = capture.stat().st_size
        while header := source.read(12):
            if len(header) != 12:
                raise ValueError("truncated column header")
            if len(columns) >= MAX_COLUMNS:
                raise ValueError(f"capture exceeds {MAX_COLUMNS} columns")
            cx, cz, count = struct.unpack("<iiI", header)
            # Reject corrupt counts before allocation or reading face payloads.
            if count > MAX_COLUMN_FACES:
                raise ValueError(f"column ({cx},{cz}) count {count} exceeds {MAX_COLUMN_FACES}")
            if count * 16 > file_size - source.tell():
                raise ValueError(f"truncated face payload for column ({cx},{cz})")
            if (cx, cz) in columns:
                raise ValueError(f"duplicate column ({cx},{cz})")
            columns.add((cx, cz))
            total += count
            previous_pass = -1
            seen = set()
            sprites = collections.Counter()
            remaining = count
            expanded_count = 0
            while remaining:
                batch_count = min(remaining, READ_BATCH_FACES)
                data = source.read(batch_count * 16)
                if len(data) != batch_count * 16:
                    raise ValueError("capture changed or was truncated during read")
                remaining -= batch_count
                for x, y, z, packed in struct.iter_unpack("<iiiI", data):
                    material, direction = packed & 65535, (packed >> 16) & 15
                    width, height = ((packed >> 20) & 31) + 1, ((packed >> 25) & 31) + 1
                    if packed >> 30:
                        fail("reserved face bits are set", column=[cx, cz])
                        continue
                    if not 0 < material < len(blocks):
                        fail("invalid material", column=[cx, cz], material=material)
                        continue
                    block = blocks[material]
                    pass_id = classes[material]
                    if width * height != 1 and (pass_id != 0 or direction >= 6 or not block["occlusion"]):
                        fail("ineligible face has merged extents", material=material, direction=direction)
                        continue
                    expanded_count += width * height
                    if expanded_count > MAX_COLUMN_FACES:
                        raise ValueError("expanded column surface exceeds unit-face bound")
                    unit_total += width * height
                    if pass_id < previous_pass:
                        fail("noncontiguous pass ranges", column=[cx, cz], previous=previous_pass, current=pass_id)
                    previous_pass = pass_id
                    counts[material] += 1
                    pass_directions[(pass_id, direction)] += 1
                    dx = width - 1 if direction >= 2 else 0
                    dy = height - 1 if direction not in (2, 3) else 0
                    dz = width - 1 if direction < 2 else height - 1 if direction < 4 else 0
                    valid_position = (cx * WIDTH <= x <= x + dx < (cx + 1) * WIDTH and
                                      cz * WIDTH <= z <= z + dz < (cz + 1) * WIDTH and
                                      MIN_Y <= y <= y + dy < MAX_Y)
                    if not valid_position:
                        fail("face outside column bounds", column=[cx, cz], position=[x, y, z])
                    allowed = range(6, 12 if block["requiresSolidBase"] else 10) if block["sprite"] else range(6)
                    if direction not in allowed:
                        fail("invalid face direction", material=material, direction=direction)
                    elif valid_position:
                        if dy and (y - MIN_Y) // 32 != (y + dy - MIN_Y) // 32:
                            fail("merged face crosses vertical tile", position=[x, y, z])
                        for v in range(height):
                            for u in range(width):
                                px = x + (u if direction >= 2 else 0)
                                py = y + (v if direction not in (2, 3) else 0)
                                pz = z + (u if direction < 2 else v if direction < 4 else 0)
                                key = (direction << 19) | ((py - MIN_Y) << 10) | ((pz - cz * WIDTH) << 5) | (px - cx * WIDTH)
                                if key in seen:
                                    fail("duplicate face", position=[px, py, pz], material=material, direction=direction)
                                seen.add(key)
                                location = (px, py, pz, material)
                                if location in fixture:
                                    actual_fixture[location].append(direction)
                    location = (x, y, z, material)
                    if block["sprite"]:
                        sprites[location] += 1
            for location, actual in sprites.items():
                expected = 6 if blocks[location[3]]["requiresSolidBase"] else 4
                if actual != expected:
                    fail("incomplete sprite", location=list(location), actual=actual, expected=expected)
    if not columns:
        raise ValueError("empty capture")
    fixture_results = []
    for location, entry in fixture.items():
        actual = sorted(actual_fixture[location])
        expected_directions = entry.get("expectedDirections")
        passed = len(actual) == entry["expectedFaces"] and (expected_directions is None or actual == sorted(expected_directions))
        fixture_results.append({"name": entry.get("name", blocks[location[3]]["id"]), "position": list(location[:3]), "material": location[3], "expected_faces": entry["expectedFaces"], "actual_faces": len(actual), "directions": actual, "passed": passed})
        if not passed:
            fail("fixture expectation mismatch", location=list(location), expected=entry["expectedFaces"], actual=actual)
    return {"capture": str(capture), "columns": len(columns), "faces": total, "unit_faces": unit_total,
            "passed": error_count == 0, "error_count": error_count, "errors": errors,
            "passes": {str(p): sum(n for m, n in counts.items() if classes[m] == p) for p in range(5)},
            "materials": {blocks[m]["id"]: n for m, n in sorted(counts.items())},
            "pass_directions": {f"{p}:{d}": n for (p, d), n in sorted(pass_directions.items())},
            "fixture": fixture_results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="WorldCapture .quads.bin file")
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--fixture", type=Path, help="optional coordinate/material expectations JSON")
    args = parser.parse_args()
    try:
        blocks = load_catalog(args.catalog)
        result = audit(args.capture, blocks, load_fixture(args.fixture, blocks))
        print(json.dumps(result, indent=2))
        return 0 if result["passed"] else 1
    except (OSError, ValueError, TypeError, KeyError, struct.error) as error:
        print(json.dumps({"passed": False, "error": str(error)}, indent=2))
        return 1


if __name__ == "__main__":
    sys.exit(main())
