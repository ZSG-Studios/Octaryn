"""Exercise production world discovery, imports, collision spawn and isolated saves."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile

from make_tile_fixture import payload

ROOT = Path(__file__).resolve().parents[2]


def unpack(data):
    length = struct.unpack_from("<I", data, 12)[0]
    return json.loads(data[20:20 + length]), data[28 + length:]


def binary(document, data):
    encoded = json.dumps(document, ensure_ascii=False).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    data += b"\0" * (-len(data) % 4)
    return (struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(data)) +
            struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
            struct.pack("<II", len(data), 0x004E4942) + data)


def fixtures(root):
    sources = root / "sources"
    sources.mkdir()
    for index in range(5):
        directory = sources / str(index)
        directory.mkdir()
        (directory / "main.glb").write_bytes(payload(index * 24, [.5, .5, .5, 1]))
    document, data = unpack(payload(0, [.5, .5, .5, 1]))
    external = sources / "external"
    external.mkdir()
    document["buffers"][0]["uri"] = "geometry%20data.bin"
    image = document["bufferViews"][document["images"][0]["bufferView"]]
    (external / "texture image.png").write_bytes(data[image["byteOffset"]:image["byteOffset"] + image["byteLength"]])
    document["images"][0] = {"uri": "texture%20image.png"}
    (external / "geometry data.bin").write_bytes(data)
    (external / "scene.gltf").write_text(json.dumps(document), encoding="utf-8")
    missing = sources / "missing"
    missing.mkdir()
    (missing / "scene.gltf").write_text(json.dumps(document), encoding="utf-8")
    (sources / "broken.glb").write_bytes(b"broken")
    (sources / "世界.glb").write_bytes(payload(0, [.5, .5, .5, 1]))
    authored = sources / "authored"
    authored.mkdir()
    (authored / "main.glb").write_bytes(payload(0, [.5, .5, .5, 1]))
    (authored / "map.json").write_text(json.dumps({"version": 1, "map": "main.glb",
                                                  "spawn": [1, 3, -9], "yaw": .5, "pitch": .1}),
                                     encoding="utf-8")
    tiled = sources / "tiled"
    tiled.mkdir()
    (tiled / "main.glb").write_bytes(payload(0, [.5, .5, .5, 1]))
    (tiled / "tiles").mkdir()
    (tiled / "tiles" / "tile.glb").write_bytes(payload(48, [.5, .5, .5, 1]))
    (tiled / "map.json").write_text(json.dumps({"version": 1, "map": "main.glb",
                                               "spawn": [49, 3, -9], "yaw": .5, "pitch": .1,
                                               "tiles": [[-12, 0, -12, 12, 6, 12], [36, 0, -12, 60, 6, 12]],
                                               "tile_files": ["main.glb", "tiles/tile.glb"]}), encoding="utf-8")
    document, data = unpack(payload(0, [.5, .5, .5, 1]))
    # Restrict collision/rendering to a vertical wall.
    document["accessors"][2]["count"] = 6
    document["accessors"][2]["byteOffset"] = 6 * 4
    (sources / "vertical.glb").write_bytes(binary(document, data))
    # Two opposite-facing planes form a 0.7 m gap: no room for the player.
    points = [(-12, 0, -12), (-12, 0, 12), (12, 0, 12), (12, 0, -12),
              (-12, .7, -12), (-12, .7, 12), (12, .7, 12), (12, .7, -12)]
    vertices = struct.pack("<24f", *(v for point in points for v in point))
    indices = struct.pack("<12I", 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6)
    blocked = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
               "nodes": [{"mesh": 0}], "buffers": [{"byteLength": len(vertices) + len(indices)}],
               "bufferViews": [{"buffer": 0, "byteLength": len(vertices)},
                               {"buffer": 0, "byteOffset": len(vertices), "byteLength": len(indices)}],
               "accessors": [{"bufferView": 0, "componentType": 5126, "count": 8, "type": "VEC3",
                              "min": [-12, 0, -12], "max": [12, .7, 12]},
                             {"bufferView": 1, "componentType": 5125, "count": 12, "type": "SCALAR"}],
               "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}]}
    (sources / "blocked.glb").write_bytes(binary(blocked, vertices + indices))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--evidence-root", type=Path, default=ROOT / "logs/tools/world-library")
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="probe-", dir=args.evidence_root.resolve()))
    fixtures(output)
    result = subprocess.run([str(args.probe.resolve()), str(output)], capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=120)
    (output / "probe.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout + result.stderr, end="")
    result.check_returncode()
    print(f"world_library_validation=passed evidence={output}")


if __name__ == "__main__":
    main()
