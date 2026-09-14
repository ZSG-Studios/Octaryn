"""Create a new isolated saved-world fixture for GPU fluid level/halo validation."""
import argparse
import json
from pathlib import Path
import sys

# Current authored-world identity from WorldGenerationPersistence.cpp, mode 0.
GENERATION = {"version": 1, "generator": "octaryn.basegame", "revision": 3, "seed": 1337, "mode": 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="new directory; existing paths are rejected")
    parser.add_argument("--base-y", type=int, default=160, help="stone platform height (default: 160)")
    args = parser.parse_args()
    root = args.output.resolve()
    if root.exists():
        raise ValueError(f"refusing to replace existing fixture: {root}")
    if not -255 <= args.base_y <= 246:
        raise ValueError("base height must leave platform/clearance inside world bounds: -255..246")
    base_y = args.base_y
    catalog_path = Path(__file__).resolve().parents[2] / "octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))["blocks"]
    ids = {(block["fluidKind"], block["fluidLevel"]): index for index, block in enumerate(catalog)
           if block["fluidKind"] in ("water", "lava")}
    edits = {}
    for z in range(-16, 9):
        for x in range(-12, 13):
            edits[x, base_y, z] = 5  # Catalog stone platform; source block data, no CPU geometry.
            for y in range(base_y + 1, base_y + 10):
                edits[x, y, z] = 0
    cases = []
    for kind, z in (("water", -5), ("lava", -9)):
        for level in range(8):
            position = (level - 4, base_y + 1, z)
            edits[position] = ids[kind, level]
            cases.append({"position": position, "kind": kind, "level": level, "block": ids[kind, level]})
        position = (-3, base_y + 2, z)
        edits[position] = ids[kind, 0]
        cases.append({"position": position, "kind": kind, "level": 0, "block": ids[kind, 0], "stacked": True})
    world = root / "world"
    world.mkdir(parents=True)
    (world / "world_generation.json").write_text(json.dumps(GENERATION, indent=2), encoding="utf-8")
    payload = {"version": 1, "blocks": [{"x": x, "y": y, "z": z, "block": block}
                                        for (x, y, z), block in sorted(edits.items())]}
    (world / "world_blocks.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    (world / "player_1.json").write_text(json.dumps({"version": 1, "x": 0, "y": base_y + 4.62, "z": 0,
        "pitch": -0.35, "yaw": 0, "block": 14}, indent=2), encoding="utf-8")
    (root / "expected.json").write_text(json.dumps({"purpose": "Explicit isolated GPU fluid fixture",
        "platformY": base_y, "clearanceY": [base_y + 1, base_y + 9], "platformXZ": [-12, 12, -16, 8],
        "crossesColumnBoundary": "x=-1/0", "requiredLevels": list(range(8)), "cases": cases}, indent=2), encoding="utf-8")
    print(f"fluid_fixture={root} cases={len(cases)}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
