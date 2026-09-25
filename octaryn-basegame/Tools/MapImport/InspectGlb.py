"""Print world-space bounds and counts from a GLB using accessor min/max.

Cheap structural check for bundled map GLBs: no geometry decode, just the
glTF node hierarchy multiplied against POSITION accessor min/max boxes.
"""

import argparse
import json
import struct
import sys
from pathlib import Path


def glb_json(path: Path) -> dict:
    data = path.read_bytes()
    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67:
        raise ValueError("not a GLB")
    offset = 12
    while offset < length:
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        if chunk_type == 0x4E4F534A:
            return json.loads(data[offset + 8 : offset + 8 + chunk_length])
        offset += 8 + chunk_length
    raise ValueError("no JSON chunk")


def mul(matrix, vector):
    x, y, z = vector
    return (
        matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12],
        matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13],
        matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14],
    )


def compose(node):
    if "matrix" in node:
        return node["matrix"]
    t = node.get("translation", [0, 0, 0])
    q = node.get("rotation", [0, 0, 0, 1])
    s = node.get("scale", [1, 1, 1])
    x, y, z, w = q
    # Column-major 4x4 from TRS (no shear needed for uniform map exports).
    return [
        (1 - 2 * y * y - 2 * z * z) * s[0], (2 * x * y + 2 * z * w) * s[0],
        (2 * x * z - 2 * y * w) * s[0], 0,
        (2 * x * y - 2 * z * w) * s[1], (1 - 2 * x * x - 2 * z * z) * s[1],
        (2 * y * z + 2 * x * w) * s[1], 0,
        (2 * x * z + 2 * y * w) * s[2], (2 * y * z - 2 * x * w) * s[2],
        (1 - 2 * x * x - 2 * y * y) * s[2], 0,
        t[0], t[1], t[2], 1,
    ]


def mat_mul(a, b):
    return [
        sum(a[i + 4 * k] * b[k + 4 * j] for k in range(4)) if i < 3 else b[3 + 4 * j]
        for j in range(4)
        for i in range(4)
    ]


def walk(nodes, index, parent, stats):
    node = nodes[index]
    world = mat_mul(parent, compose(node))
    if "mesh" in node:
        stats["meshes"] += 1
        for primitive in stats["doc"]["meshes"][node["mesh"]]["primitives"]:
            stats["primitives"] += 1
            accessor = stats["doc"]["accessors"][primitive["attributes"]["POSITION"]]
            low = accessor.get("min")
            high = accessor.get("max")
            if low and high:
                for point in (
                    (low[0], low[1], low[2]),
                    (high[0], low[1], low[2]),
                    (low[0], high[1], low[2]),
                    (high[0], high[1], low[2]),
                    (low[0], low[1], high[2]),
                    (high[0], low[1], high[2]),
                    (low[0], high[1], high[2]),
                    (high[0], high[1], high[2]),
                ):
                    world_point = mul(world, point)
                    for axis in range(3):
                        stats["min"][axis] = min(stats["min"][axis], world_point[axis])
                        stats["max"][axis] = max(stats["max"][axis], world_point[axis])
    for child in node.get("children", []):
        walk(nodes, child, world, stats)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("glb", type=Path)
    args = parser.parse_args()
    doc = glb_json(args.glb)
    stats = {
        "doc": doc,
        "meshes": 0,
        "primitives": 0,
        "min": [float("inf")] * 3,
        "max": [float("-inf")] * 3,
    }
    scene = doc.get("scene", 0)
    for root in doc["scenes"][scene]["nodes"]:
        walk(doc["nodes"], root, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], stats)
    triangles = 0
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            if "indices" in primitive:
                triangles += doc["accessors"][primitive["indices"]]["count"] // 3
            else:
                count = doc["accessors"][primitive["attributes"]["POSITION"]]["count"]
                triangles += count // 3
    images = len(doc.get("images", []))
    materials = len(doc.get("materials", []))
    print(
        f"bounds min={[round(v, 2) for v in stats['min']]} "
        f"max={[round(v, 2) for v in stats['max']]}"
    )
    print(
        f"nodes_with_mesh={stats['meshes']} primitives={stats['primitives']} "
        f"materials={materials} images={images} triangles={triangles}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
