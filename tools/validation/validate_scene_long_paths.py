"""Use real Unicode source and long native scene/order/cache paths without OS changes."""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from validate_meshopt_authority import State, Input, Tick


def verify(args):
    root = args.output.resolve()
    source_root = root / ("source-" + "s" * 96) / ("nested-" + "n" * 96) / "世界"
    cache_root = root / ("cache-" + "c" * 96) / ("nested-" + "n" * 96)
    source_root.mkdir(parents=True, exist_ok=True)
    cache_root.mkdir(parents=True, exist_ok=True)
    original = args.fixture.resolve()
    document = json.loads((original / "scene.json").read_text(encoding="utf-8"))
    prior_hashes = [part["hash"] for part in document["parts"]] if all(
        primitive.get("triangle_order_hash") for primitive in document["primitives"]) else None
    old_source = Path(document["source"])
    for resource in document["resources"]:
        old_path = Path(resource["path"])
        new_path = source_root / old_path.name
        shutil.copyfile(old_path, new_path)
        resource["path"] = str(new_path)
    source = source_root / old_source.name
    original_bytes = source.read_bytes()
    document["source"] = str(source)
    identity = document["source_hash"]
    for primitive in document["primitives"]:
        primitive["triangle_order"] = primitive["triangle_order_hash"] = ""
    for part in document["parts"]:
        part["geometry"] = part["hash"] = ""
        part["clusters"] = part["pages"] = part["root_pages"] = 0
    catalog = cache_root / "scene.json"
    catalog.write_text(json.dumps(document), encoding="utf-8")
    assert len(str(source)) > 260 and len(str(catalog)) > 260
    for action in ("--order", "--cook", "--cook"):
        subprocess.run([str(args.cook.resolve()), action, str(catalog)], check=True)
    cooked = json.loads(catalog.read_text(encoding="utf-8"))
    assert cooked["source_hash"] == identity and source.read_bytes() == original_bytes
    assert all(part["geometry"] for part in cooked["parts"])
    assert all(primitive["triangle_order_hash"] for primitive in cooked["primitives"])
    if prior_hashes:
        assert [part["hash"] for part in cooked["parts"]] == prior_hashes, "path expansion changed geometry identity"
    library = args.library.resolve()
    searches = [os.add_dll_directory(str(path.resolve())) for path in
                [library.parent, *args.dependency_directory]] if os.name == "nt" else []
    native = c.CDLL(str(library))
    create = native.octaryn_server_map_world_create
    create.argtypes, create.restype = [c.c_char_p, c.c_char_p], c.c_void_p
    destroy = native.octaryn_server_map_world_destroy
    destroy.argtypes = [c.c_void_p]
    count = native.octaryn_server_map_world_triangle_count
    count.argtypes, count.restype = [c.c_void_p], c.c_uint64
    step = native.octaryn_server_map_world_step
    step.argtypes = [c.c_void_p, c.POINTER(Input), c.c_double, c.POINTER(State), c.POINTER(Tick)]
    poses = []
    for ordered in (False, True):
        manifest = root / ("ordered.json" if ordered else "source.json")
        settings = dict(version=1, map=str(source), spawn=[0, 1.62, 6], yaw=0, pitch=0)
        if ordered:
            settings["scene_catalog"] = str(catalog)
        manifest.write_text(json.dumps(settings), encoding="utf-8")
        handle = create(str(source).encode("utf-8"), str(manifest).encode("utf-8"))
        assert handle, "long Unicode source or long ordered catalog authority failed"
        try:
            assert count(handle) == (2 if ordered else document["instanced_triangles"])
            state, control, tick = State(), Input(), Tick()
            state.y, state.z = 2, 6
            for _ in range(120):
                assert step(handle, c.byref(control), 1 / 60, c.byref(state), c.byref(tick)) == 0
            assert state.grounded
            poses.append((state.x, state.y, state.z))
        finally:
            destroy(handle)
    assert poses[0] == poses[1], "ordered long-path collision changed original pose"
    for search in searches:
        search.close()
    print(f"scene_long_paths=passed source_chars={len(str(source))} catalog_chars={len(str(catalog))} "
          f"unicode_source=1 source_identity_unchanged=1 same_geometry_hash={int(bool(prior_hashes))} "
          "ordered_cook=1 cache_reuse=1 authority_parity=1", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("library", "cook", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--dependency-directory", type=Path, action="append", default=[])
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    if args.worker:
        verify(args)
        return
    args.output.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([sys.executable, __file__, *sys.argv[1:], "--worker"], capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=90)
    (args.output / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout + result.stderr, end="")
    result.check_returncode()


if __name__ == "__main__":
    main()
