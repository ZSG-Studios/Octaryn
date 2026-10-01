#!/usr/bin/env python3
"""Generate three 24 m GLB cells with distinct materials and known collision floors."""
import argparse
import base64
import json
from pathlib import Path
import struct


def payload(center, color):
    positions, normals, indices = [], [], []

    def quad(points, normal):
        a = [points[1][axis] - points[0][axis] for axis in range(3)]
        b = [points[2][axis] - points[0][axis] for axis in range(3)]
        cross = (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
        if sum(cross[axis]*normal[axis] for axis in range(3)) < 0:
            points = [points[0], points[3], points[2], points[1]]
        first = len(positions)
        positions.extend(points)
        normals.extend([normal] * 4)
        indices.extend((first, first + 1, first + 2, first, first + 2, first + 3))

    quad([(center - 12, 0, -12), (center - 12, 0, 12),
          (center + 12, 0, 12), (center + 12, 0, -12)], (0, 1, 0))
    x, z = center + 3, 3
    quad([(x-2, 0, z-2), (x+2, 0, z-2), (x+2, 5, z-2), (x-2, 5, z-2)], (0, 0, -1))
    quad([(x+2, 0, z+2), (x-2, 0, z+2), (x-2, 5, z+2), (x+2, 5, z+2)], (0, 0, 1))
    quad([(x-2, 0, z+2), (x-2, 0, z-2), (x-2, 5, z-2), (x-2, 5, z+2)], (-1, 0, 0))
    quad([(x+2, 0, z-2), (x+2, 0, z+2), (x+2, 5, z+2), (x+2, 5, z-2)], (1, 0, 0))
    quad([(x-2, 5, z-2), (x+2, 5, z-2), (x+2, 5, z+2), (x-2, 5, z+2)], (0, 1, 0))
    data = bytearray()
    views = []

    def append(content):
        while len(data) % 4:
            data.append(0)
        views.append({"buffer": 0, "byteOffset": len(data), "byteLength": len(content)})
        data.extend(content)
        return len(views) - 1

    p = append(struct.pack("<" + "f" * len(positions) * 3, *(v for point in positions for v in point)))
    n = append(struct.pack("<" + "f" * len(normals) * 3, *(v for point in normals for v in point)))
    i = append(struct.pack("<" + "I" * len(indices), *indices))
    uv = append(struct.pack("<" + "f" * len(positions) * 2,
                            *[v for _ in range(len(positions) // 4)
                              for point in ((0, 0), (1, 0), (1, 1), (0, 1)) for v in point]))
    # Identical embedded texels across tiles positively exercise shared-image ownership.
    png = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4////fwAJ+wP9KobjigAAAABJRU5ErkJggg==")
    image = append(png)
    doc = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
           "nodes": [{"mesh": 0}], "buffers": [{"byteLength": len(data)}], "bufferViews": views,
           "accessors": [{"bufferView": p, "componentType": 5126, "count": len(positions), "type": "VEC3",
                          "min": [min(v[a] for v in positions) for a in range(3)],
                          "max": [max(v[a] for v in positions) for a in range(3)]},
                         {"bufferView": n, "componentType": 5126, "count": len(normals), "type": "VEC3"},
                         {"bufferView": i, "componentType": 5125, "count": len(indices), "type": "SCALAR"},
                         {"bufferView": uv, "componentType": 5126, "count": len(positions), "type": "VEC2"}],
           "images": [{"bufferView": image, "mimeType": "image/png"}], "textures": [{"source": 0}],
           "materials": [{"doubleSided": True, "pbrMetallicRoughness": {"baseColorFactor": color,
                          "baseColorTexture": {"index": 0}, "metallicFactor": .2, "roughnessFactor": .35}}],
           "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 3}, "indices": 2, "material": 0}]}]}
    encoded = json.dumps(doc, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    data += b"\0" * (-len(data) % 4)
    return struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(data)) + \
        struct.pack("<II", len(encoded), 0x4E4F534A) + encoded + struct.pack("<II", len(data), 0x004E4942) + data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--tiles", type=int, default=3)
    args = parser.parse_args()
    if not 3 <= args.tiles <= 129:
        parser.error("--tiles must be between 3 and 129")
    args.output.mkdir(parents=True, exist_ok=True)
    colors = ([.8, .15, .1, 1], [.1, .65, .2, 1], [.1, .25, .85, 1])
    for index in range(args.tiles):
        color = colors[index % len(colors)]
        (args.output / f"tile-{index}.glb").write_bytes(payload(index * 24, color))
    manifest = {"version": 1, "map": "tile-0.glb", "spawn": [0, 3, -9], "yaw": 3.14159265, "pitch": -.15,
                "tiles": [[i*24-12, -1, -12, i*24+12, 6, 12] for i in range(args.tiles)],
                "tile_files": [f"tile-{i}.glb" for i in range(args.tiles)]}
    (args.output / "map.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps({"manifest": str((args.output / "map.json").resolve()), "tiles": args.tiles,
                      "triangles_per_tile": 12, "authority_collision": "runtime server residency policy"}))


if __name__ == "__main__":
    main()
