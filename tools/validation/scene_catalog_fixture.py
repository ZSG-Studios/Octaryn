"""Author an original two-instance opaque/masked/blended scene for runtime comparison."""
import argparse
import copy
import json
from pathlib import Path

from reflection_fixture import generate as reflection_fixture


def generate(directory):
    reflection_fixture(directory)
    source = directory / "reflection.gltf"
    document = json.loads(source.read_text(encoding="utf-8"))
    document["asset"]["generator"] = "ZSG shared-geometry instance qualification"
    document["nodes"] = [dict(mesh=0, name="Original"),
                         dict(mesh=0, name="Mirrored", translation=[16, 0, 0], scale=[-1, 1, 1])]
    document["scenes"][0]["nodes"] = [0, 1]
    document["materials"].append(dict(name="Blue glass", alphaMode="BLEND", doubleSided=True,
        pbrMetallicRoughness=dict(baseColorFactor=[.1, .4, 1, .35], metallicFactor=0, roughnessFactor=.2)))
    blended = copy.deepcopy(document["meshes"][0]["primitives"][3])
    blended["material"] = 4
    document["meshes"][0]["primitives"].append(blended)
    source.write_text(json.dumps(document), encoding="utf-8")
    manifest = dict(version=1, map=source.name, spawn=[0, 1.62, 6], yaw=0, pitch=-.85)
    (directory / "source-map.json").write_text(json.dumps(manifest), encoding="utf-8")
    manifest["scene_catalog"] = "scene.json"
    (directory / "map.json").write_text(json.dumps(manifest), encoding="utf-8")
    evidence = dict(license="CC0-1.0", unique_meshes=1, instances=2, mirrored_instances=1,
                    unique_triangles=10, instanced_triangles=20, parts=5,
                    materials=["opaque", "mask", "blend"], source_collision="reflection.gltf")
    (directory / "fixture.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    return directory / "map.json"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generate", type=Path, required=True)
    args = parser.parse_args()
    print(generate(args.generate.resolve()))
