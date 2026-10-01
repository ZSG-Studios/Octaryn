"""Original CC0 mirror stations for incoming scene-part publication checks."""
import argparse
import copy
import json
from pathlib import Path
import struct

from reflection_fixture import generate as generate_mirror


DISTANCE = 260


def route_definition():
    return dict(version=1, phases=[
        dict(name="before", seconds=12, yaw=0, pitch=-.85),
        dict(name="travel", seconds=31, forward=1, sprint=True, target=[248, 6],
             tolerance=1.2, min_distance=246, pitch=-.15),
        dict(name="approach", seconds=5, forward=1, target=[DISTANCE, 6],
             tolerance=.75, min_distance=10, pitch=-.15),
        dict(name="after", seconds=12, yaw=0, pitch=-.85)])


def generate(directory):
    directory = Path(directory)
    generate_mirror(directory)
    source = directory / "reflection.gltf"
    document = json.loads(source.read_text(encoding="utf-8"))
    blob = bytearray((directory / "reflection.bin").read_bytes())

    def attribute(values, kind, components, component_type=5126):
        while len(blob) % 4:
            blob.append(0)
        first = len(blob)
        code = "f" if component_type == 5126 else "I"
        for value in values:
            blob.extend(struct.pack("<" + code * components, *value))
        document["bufferViews"].append(dict(buffer=0, byteOffset=first, byteLength=len(blob)-first))
        item = dict(bufferView=len(document["bufferViews"])-1, componentType=component_type,
                    count=len(values), type=kind)
        if kind == "VEC3":
            item.update(min=[min(value[i] for value in values) for i in range(3)],
                        max=[max(value[i] for value in values) for i in range(3)])
        document["accessors"].append(item)
        return len(document["accessors"])-1

    # Separate source meshes require new part owners, not only another instance
    # of an already resident owner. Every source triangle is retained.
    document["meshes"].append(copy.deepcopy(document["meshes"][0]))
    positions = attribute([(8, 0, 12), (DISTANCE-8, 0, 12),
                           (DISTANCE-8, 0, -12), (8, 0, -12)], "VEC3", 3)
    normals = attribute([(0, 1, 0)] * 4, "VEC3", 3)
    indices = attribute([(i,) for i in (0, 1, 2, 0, 2, 3)], "SCALAR", 1, 5125)
    document["materials"].append(dict(name="Corridor", doubleSided=True,
        pbrMetallicRoughness=dict(baseColorFactor=[.3, .3, .3, 1], metallicFactor=0, roughnessFactor=.9)))
    document["meshes"].append(dict(primitives=[dict(attributes=dict(POSITION=positions, NORMAL=normals),
                                                   indices=indices, material=4)]))
    document["nodes"] = [dict(mesh=0, name="Start mirror"),
                         dict(mesh=1, name="Incoming mirror", translation=[DISTANCE, 0, 0]),
                         dict(mesh=2, name="Continuous authority floor")]
    document["scenes"][0]["nodes"] = [0, 1, 2]
    document["buffers"][0]["byteLength"] = len(blob)
    document["asset"]["generator"] = "ZSG incoming scene-part qualification (CC0-1.0)"
    source.write_text(json.dumps(document), encoding="utf-8")
    (directory / "reflection.bin").write_bytes(blob)
    manifest = dict(version=1, map=source.name, scene_catalog="scene.json",
                    spawn=[0, 1.62, 6], yaw=0, pitch=-.85)
    (directory / "map.json").write_text(json.dumps(manifest), encoding="utf-8")
    route = route_definition()
    (directory / "route.json").write_text(json.dumps(route, indent=2), encoding="utf-8")
    evidence = dict(license="CC0-1.0", unique_meshes=3, instances=3, unique_triangles=18,
                    instanced_triangles=18, parts=9, station_distance=DISTANCE,
                    load_radius=128, keep_radius=160, render_budget_bytes=512*1024**2,
                    route_seconds=60, expected=["new part owners enter the normal load radius",
                    "old roots and rays stay published during incoming preparation",
                    "retired owners wait for their consumer fences",
                    "red and green reflections before and after; masked blue absent"],
                    scope="Small mechanism fixture; does not qualify large-scene scale or performance")
    (directory / "fixture.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    return directory / "map.json"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generate", type=Path, required=True)
    print(generate(parser.parse_args().generate.resolve()))
