"""Compare GPU fluid heights/flow with the original algorithm and captured source voxels."""
import argparse
import collections
import json
import math
from pathlib import Path
import struct
import sys

HEADER = struct.Struct("<iiI")
RECORD = struct.Struct("<iiiI8f27H")
MAX_COLUMNS = 4225
MAX_COLUMN_FACES = 32 * 32 * 512 * 6
CATALOG = Path(__file__).resolve().parents[2] / "octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json"


def exact(stream, size):
    data = stream.read(size)
    if len(data) != size:
        raise ValueError("truncated fluid capture")
    return data


def expected(blocks, center, neighborhood):
    """Independent scalar port of original world/chunks/build_mesh.cpp."""
    kind = blocks[center]["fluidKind"]

    def sample(x, y, z):
        return blocks[neighborhood[x + 1 + 3 * (y + 1 + 3 * (z + 1))]]

    def height(x, y, z):
        block = sample(x, y, z)
        if block["fluidKind"] != kind:
            return -1.0 if block["solid"] else 0.0
        if sample(x, y + 1, z)["fluidKind"] == kind:
            return 1.0
        return max(1, min(8, 8 - block["fluidLevel"])) / 9.0

    def corner(dx, dz):
        own, side_x, side_z = height(0, 0, 0), height(dx, 0, 0), height(0, 0, dz)
        if side_x >= 1 or side_z >= 1:
            return 1.0
        values = [own, side_x, side_z]
        if side_x > 0 or side_z > 0:
            diagonal = height(dx, 0, dz)
            if diagonal >= 1:
                return 1.0
            values.append(diagonal)
        weighted = [(value, 10 if value >= 0.8 else 1) for value in values if value >= 0]
        return sum(value * weight for value, weight in weighted) / sum(weight for _, weight in weighted)

    heights = [corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, 1)]
    flow_x = flow_z = 0.0
    for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        neighbor = sample(dx, 0, dz)
        pull = 0.0
        if neighbor["fluidKind"] == kind:
            pull = height(0, 0, 0) - height(dx, 0, dz)
        elif not neighbor["occlusion"] and sample(dx, -1, dz)["fluidKind"] == kind:
            pull = height(0, 0, 0) - (height(dx, -1, dz) - 8 / 9)
        flow_x += dx * pull
        flow_z += dz * pull
    magnitude = math.hypot(flow_x, flow_z)
    flow = [0.0, 0.0, 0.0] if magnitude <= 0.0001 else [
        flow_x / magnitude, flow_z / magnitude, max(1, min(15, math.floor(magnitude * 15 + 0.5)))
    ]
    return heights + flow + [float(sample(0, 1, 0)["fluidKind"] == kind)]


def validate(path, catalog, required_levels, fixture=None):
    document = json.loads(catalog.read_text(encoding="utf-8"))
    if document.get("schema") != "octaryn.basegame.blocks.v1":
        raise ValueError("unexpected block catalog schema")
    blocks = document["blocks"]
    if not 1 <= len(blocks) <= 65536:
        raise ValueError("invalid block catalog size")
    levels, kinds = collections.Counter(), collections.Counter()
    errors, error_count, count, max_error = [], 0, 0, 0.0
    coordinates = set()
    cases = json.loads(fixture.read_text(encoding="utf-8"))["cases"] if fixture else []
    if len(cases) > 1024:
        raise ValueError("fixture case limit exceeded")
    required_cases = {(tuple(case["position"]), case["block"]) for case in cases}
    seen_cases = set()
    with path.open("rb") as stream:
        if exact(stream, 8) != b"OCFLUID1":
            raise ValueError("unexpected fluid capture magic")
        columns = struct.unpack("<I", exact(stream, 4))[0]
        if not 1 <= columns <= MAX_COLUMNS:
            raise ValueError("fluid column count exceeds bounds")
        file_size = path.stat().st_size
        for _ in range(columns):
            cx, cz, faces = HEADER.unpack(exact(stream, HEADER.size))
            if (cx, cz) in coordinates or faces > MAX_COLUMN_FACES:
                raise ValueError("duplicate column or excessive fluid face count")
            coordinates.add((cx, cz))
            if faces * RECORD.size > file_size - stream.tell():
                raise ValueError("fluid face count exceeds remaining file")
            for _ in range(faces):
                row = RECORD.unpack(exact(stream, RECORD.size))
                x, y, z, packed = row[:4]
                values, neighborhood = row[4:12], row[12:]
                block, direction = packed & 65535, packed >> 16
                if not cx * 32 <= x < cx * 32 + 32 or not cz * 32 <= z < cz * 32 + 32 or not -256 <= y <= 255:
                    raise ValueError("fluid face lies outside column/world bounds")
                if direction >= 6 or block >= len(blocks) or any(item >= len(blocks) for item in neighborhood):
                    raise ValueError("invalid fluid direction or block ID")
                if neighborhood[13] != block or blocks[block]["fluidKind"] not in ("water", "lava"):
                    raise ValueError("GPU fluid face disagrees with authoritative center voxel")
                reference = expected(blocks, block, neighborhood)
                differences = [abs(a - b) for a, b in zip(values, reference)]
                error = max(differences)
                if not all(math.isfinite(value) for value in values) or error > 0.0001:
                    error_count += 1
                    if len(errors) < 20:
                        errors.append({"position": [x, y, z], "block": block, "direction": direction,
                                       "gpu": values, "expected": reference})
                else:
                    max_error = max(max_error, error)
                case = ((x, y, z), block)
                if case in required_cases:
                    seen_cases.add(case)
                count += 1
                levels[blocks[block]["fluidLevel"]] += 1
                kinds[blocks[block]["fluidKind"]] += 1
        if stream.read(1):
            raise ValueError("trailing fluid capture data")
    missing = sorted(set(required_levels) - levels.keys())
    missing_cases = sorted(required_cases - seen_cases)
    return {"passed": bool(count) and not error_count and not missing and not missing_cases, "columns": columns,
            "fluid_faces": count, "kinds": dict(kinds), "levels": dict(sorted(levels.items())),
            "max_absolute_error": max_error, "error_count": error_count, "errors": errors,
            "missing_required_levels": missing, "fixture_cases": len(seen_cases), "missing_fixture_cases": missing_cases, "capture": str(path.resolve())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="GPU .fluids.bin capture")
    parser.add_argument("--fixture", type=Path, help="expected.json from create_fluid_fixture.py")
    parser.add_argument("--catalog", type=Path, default=CATALOG)
    parser.add_argument("--require-level", type=int, choices=range(8), action="append", default=[])
    args = parser.parse_args()
    try:
        result = validate(args.capture, args.catalog, args.require_level, args.fixture)
    except (OSError, ValueError, KeyError, TypeError, ZeroDivisionError) as error:
        result = {"passed": False, "error": str(error)}
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
