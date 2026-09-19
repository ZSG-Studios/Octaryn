#!/usr/bin/env python3
"""Generate the placeholder GLB map plus its manifest.

The placeholder gives the map-world mode a playable world before a real
Blender export is bundled. Replace Assets/Maps/main.glb + map.json with a
Blender glTF 2.0 export (+Y up) using the same manifest schema.
"""

import json
import struct
import sys
from pathlib import Path

ROUGHNESS = 0.85
METALLIC = 0.0


class Primitive:
    def __init__(self, material_index):
        self.material_index = material_index
        self.positions = []
        self.normals = []

    def triangle(self, a, b, c, normal):
        for point in (a, b, c):
            self.positions.extend(point)
            self.normals.extend(normal)

    def quad(self, a, b, c, d, normal):
        self.triangle(a, b, c, normal)
        self.triangle(a, c, d, normal)

    def box(self, center, half):
        cx, cy, cz = center
        hx, hy, hz = half
        lo = (cx - hx, cy - hy, cz - hz)
        hi = (cx + hx, cy + hy, cz + hz)
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        self.quad((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (0, 0, 1))
        self.quad((x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (0, 0, -1))
        self.quad((x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (1, 0, 0))
        self.quad((x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (-1, 0, 0))
        self.quad((x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0), (0, 1, 0))
        self.quad((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), (0, -1, 0))

    def wedge(self, center, size):
        """Right ramp rising toward -Z: walkable slope for movement checks."""
        cx, cy, cz = center
        hx, hy, hz = size
        x0, x1 = cx - hx, cx + hx
        y0, y1 = cy - hy, cy + hy
        znear, zfar = cz + hz, cz - hz
        high = y1
        top = (0.0, 0.86, 0.5)
        self.quad((x0, y0, znear), (x1, y0, znear), (x1, high, zfar), (x0, high, zfar), top)
        self.quad((x0, y0, zfar), (x1, y0, zfar), (x1, y0, znear), (x0, y0, znear), (0, -1, 0))
        self.quad((x1, y0, znear), (x1, y0, zfar), (x1, high, zfar), (x1, y0, znear), (1, 0, 0))
        self.quad((x0, y0, zfar), (x0, y0, znear), (x0, y0, znear), (x0, high, zfar), (-1, 0, 0))
        self.quad((x0, high, zfar), (x1, high, zfar), (x1, y0, zfar), (x0, y0, zfar), (0, 0, -1))


def build_world():
    ground = Primitive(0)
    span = 96.0
    ground.quad((-span, 0, span), (span, 0, span), (span, 0, -span), (-span, 0, -span), (0, 1, 0))
    stone = Primitive(1)
    stone.box((0, 1.5, -6), (3, 1.5, 3))
    stone.box((-14, 2, -18), (1.2, 2, 1.2))
    stone.box((14, 2, -18), (1.2, 2, 1.2))
    stone.box((-14, 2, 6), (1.2, 2, 1.2))
    stone.box((14, 2, 6), (1.2, 2, 1.2))
    stone.box((0, 4.2, -24), (16, 0.6, 1.2))
    accent = Primitive(2)
    accent.box((-7, 1, -12), (1.5, 1, 1.5))
    accent.box((7, 1, -12), (1.5, 1, 1.5))
    accent.box((0, 0.75, 2), (2, 0.75, 2))
    ramp = Primitive(3)
    ramp.wedge((-22, 1, -6), (3, 1, 6))
    ramp.wedge((22, 1, -6), (3, 1, 6))
    return [ground, stone, accent, ramp]


def main():
    root = Path(__file__).resolve().parents[2] / "Assets" / "Maps"
    root.mkdir(parents=True, exist_ok=True)
    primitives = build_world()
    blob = bytearray()
    buffer_views = []
    accessors = []

    def add_view(data, target):
        offset = len(blob)
        blob.extend(data)
        buffer_views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target})
        return len(buffer_views) - 1

    mesh_primitives = []
    for prim in primitives:
        count = len(prim.positions) // 3
        position_view = add_view(struct.pack(f"<{len(prim.positions)}f", *prim.positions), 34962)
        normal_view = add_view(struct.pack(f"<{len(prim.normals)}f", *prim.normals), 34962)
        uvs = [0.0, 0.0] * count
        uv_view = add_view(struct.pack(f"<{len(uvs)}f", *uvs), 34962)
        xs = prim.positions[0::3]
        ys = prim.positions[1::3]
        zs = prim.positions[2::3]
        accessors.append({"bufferView": position_view, "componentType": 5126, "count": count,
                          "type": "VEC3", "min": [min(xs), min(ys), min(zs)], "max": [max(xs), max(ys), max(zs)]})
        position_accessor = len(accessors) - 1
        accessors.append({"bufferView": normal_view, "componentType": 5126, "count": count, "type": "VEC3"})
        normal_accessor = len(accessors) - 1
        accessors.append({"bufferView": uv_view, "componentType": 5126, "count": count, "type": "VEC2"})
        uv_accessor = len(accessors) - 1
        mesh_primitives.append({"attributes": {"POSITION": position_accessor, "NORMAL": normal_accessor,
                                               "TEXCOORD_0": uv_accessor},
                                "material": prim.material_index, "mode": 4})

    materials = [
        {"name": "ground", "pbrMetallicRoughness": {"baseColorFactor": [0.42, 0.52, 0.38, 1.0],
                                                    "metallicFactor": METALLIC, "roughnessFactor": ROUGHNESS}},
        {"name": "stone", "pbrMetallicRoughness": {"baseColorFactor": [0.62, 0.60, 0.56, 1.0],
                                                   "metallicFactor": METALLIC, "roughnessFactor": ROUGHNESS}},
        {"name": "accent", "pbrMetallicRoughness": {"baseColorFactor": [0.72, 0.35, 0.25, 1.0],
                                                    "metallicFactor": METALLIC, "roughnessFactor": ROUGHNESS}},
        {"name": "ramp", "pbrMetallicRoughness": {"baseColorFactor": [0.50, 0.48, 0.46, 1.0],
                                                  "metallicFactor": METALLIC, "roughnessFactor": ROUGHNESS}},
    ]

    gltf = {
        "asset": {"version": "2.0", "generator": "octaryn-map-placeholder"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "placeholder_map"}],
        "meshes": [{"name": "map", "primitives": mesh_primitives}],
        "materials": materials,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(blob)}],
    }

    json_data = json.dumps(gltf, separators=(",", ":")).encode()
    while len(json_data) % 4:
        json_data += b" "
    while len(blob) % 4:
        blob += b"\0"
    total = 12 + 8 + len(json_data) + 8 + len(blob)
    glb = struct.pack("<III", 0x46546C67, 2, total)
    glb += struct.pack("<II", len(json_data), 0x4E4F534A) + json_data
    glb += struct.pack("<II", len(blob), 0x004E4942) + bytes(blob)
    glb_path = root / "main.glb"
    glb_path.write_bytes(glb)

    manifest = {
        "version": 1,
        "map": "main.glb",
        "spawn": [0.0, 1.8, 12.0],
        "yaw": 0.0,
        "pitch": -0.2,
    }
    (root / "map.json").write_text(json.dumps(manifest, indent=2) + "\n")
    triangles = sum(len(p.positions) // 9 for p in primitives)
    print(f"wrote {glb_path} ({total} bytes, {triangles} triangles) and map.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
