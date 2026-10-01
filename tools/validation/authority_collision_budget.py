"""Inspect cooked tile JSON headers to estimate protected collision reservations."""
import argparse
import json
from pathlib import Path
import struct


def inspect(manifest):
    world = json.loads(manifest.read_text())
    rows = []
    for bounds, relative in zip(world["tiles"], world["tile_files"]):
        with (manifest.parent / relative).open("rb") as source:
            header = source.read(20)
            magic, version, _, size, kind = struct.unpack("<IIIII", header)
            if (magic, version, kind) != (0x46546C67, 2, 0x4E4F534A) or size > 64*1024**2:
                raise ValueError("Invalid or oversized GLB header")
            data = json.loads(source.read(size))
        # This tool accepts the cooker's one untransformed mesh node. General
        # instanced glTF needs full scene traversal to predict decoded geometry.
        if len(data["nodes"]) != 1 or data["nodes"][0] != {"mesh": 0}:
            raise ValueError("Expected one untransformed cooked mesh")
        triangles = vertices = 0
        for mesh in data["meshes"]:
            for primitive in mesh["primitives"]:
                if primitive.get("mode", 4) != 4:
                    raise ValueError("Expected cooked triangle lists")
                triangles += data["accessors"][primitive["indices"]]["count"] // 3
                vertices += data["accessors"][primitive["attributes"]["POSITION"]]["count"]
        rows.append({"bounds": bounds, "triangles": triangles, "vertices": vertices,
                     "bytes": triangles * 256 + vertices * 12 + 65536})
    x, _, z = world["spawn"]
    protected = []
    for radius in (4, 8, 10, 16, 32, 48):
        selected = [row for row in rows if row["bounds"][0] <= x+radius and row["bounds"][3] >= x-radius
                    and row["bounds"][2] <= z+radius and row["bounds"][5] >= z-radius]
        protected.append({"radius": radius, "tiles": len(selected), "bytes": sum(row["bytes"] for row in selected)})
    centers = []
    for row in rows:
        b = row["bounds"]
        cx, cz = (b[0]+b[3])/2, (b[2]+b[5])/2
        selected = [r for r in rows if r["bounds"][0] <= cx+4 and r["bounds"][3] >= cx-4
                    and r["bounds"][2] <= cz+4 and r["bounds"][5] >= cz-4]
        centers.append(dict(x=cx, z=cz, radius=4, tiles=len(selected), bytes=sum(r["bytes"] for r in selected)))
    return {"manifest": str(manifest.resolve()), "total_tiles": len(rows), "spawn": world["spawn"],
            "reservation_method": "256 bytes/triangle + 12 bytes/decoded vertex + 65536 bytes/tile; not measured RSS",
            "protected": protected, "maximum_sampled_tile_center": max(centers, key=lambda row: row["bytes"]),
            "coverage": "spawn radii and tile-center samples only; not a global worst-case proof"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    arguments.output.write_text(json.dumps(inspect(arguments.manifest), indent=2))
