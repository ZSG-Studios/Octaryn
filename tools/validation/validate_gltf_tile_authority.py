"""Exercise bounded external-buffer tiles through the production authority DLL."""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from validate_meshopt_authority import State, Input, Tick, Hit


class Stats(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("version", "resident", "preparing", "failed")]
    _fields_ += [(name, c.c_uint64) for name in
                ("reserved", "loads", "evictions", "cancelled", "waits", "bytes", "budget")]


def verify(args):
    library = args.library.resolve()
    searches = []
    if os.name == "nt":
        searches = [os.add_dll_directory(str(path.resolve())) for path in
                    [library.parent, *args.dependency_directory]]
    native = c.CDLL(str(library))
    create = native.octaryn_server_map_world_create
    create.argtypes, create.restype = [c.c_char_p, c.c_char_p], c.c_void_p
    destroy = native.octaryn_server_map_world_destroy
    destroy.argtypes = [c.c_void_p]
    count = native.octaryn_server_map_world_triangle_count
    count.argtypes, count.restype = [c.c_void_p], c.c_uint64
    stats = native.octaryn_server_map_world_collision_stats
    stats.argtypes = [c.c_void_p, c.POINTER(Stats), c.c_uint32]
    spawn = native.octaryn_server_map_world_spawn
    spawn.argtypes = [c.c_void_p, c.POINTER(State)]
    step = native.octaryn_server_map_world_step
    step.argtypes = [c.c_void_p, c.POINTER(Input), c.c_double, c.POINTER(State), c.POINTER(Tick)]
    ray = native.octaryn_server_map_world_raycast
    ray.argtypes = [c.c_void_p] + [c.c_float] * 7 + [c.POINTER(Hit)]
    manifest = args.manifest.resolve()
    description = json.loads(manifest.read_text(encoding="utf-8"))
    assert len(description["tile_files"]) == 2, "requires the two-tile reflection fixture"
    source = manifest.parent / description["map"]
    handle = create(str(source).encode(), str(manifest).encode())
    assert handle, "authority rejected external-buffer tiled glTF"
    try:
        residency = Stats()
        assert stats(handle, c.byref(residency), c.sizeof(residency)) == 0
        assert residency.version == 2 and residency.resident == 2 and residency.failed == 0
        assert residency.bytes <= residency.reserved <= residency.budget
        assert count(handle) == 16, "tile instances or primitives were lost"
        for x in (0, 16):
            hit = Hit()
            assert ray(handle, x, 5, 6, 0, -1, 0, 10, c.byref(hit)) == 0
            assert hit.hit and abs(hit.y) < 1e-5 and abs(hit.distance - 5) < 1e-5
            state, control, tick = State(), Input(), Tick()
            assert spawn(handle, c.byref(state)) == 0
            state.x, state.y, state.z = x, 3, 6
            for _ in range(240):
                assert step(handle, c.byref(control), 1 / 60, c.byref(state), c.byref(tick)) == 0
            assert state.grounded and abs(state.y - 1.62) < .05, (state.y, state.grounded)
    finally:
        destroy(handle)
    broken = args.output.resolve() / "truncated"
    shutil.copytree(manifest.parent, broken, dirs_exist_ok=True)
    first = broken / description["tile_files"][0]
    resource = json.loads(first.read_text(encoding="utf-8"))["buffers"][0]["uri"]
    (first.parent / resource).write_bytes(b"\0")
    assert not create(str(broken / description["map"]).encode(),
                      str(broken / manifest.name).encode()), "truncated external buffer accepted"
    print("gltf_tile_authority=passed tiles=2 triangles=16 grounded=2 rays=2 "
          f"truncated_rejected=1 resident_bytes={residency.bytes} budget_bytes={residency.budget}", flush=True)
    for search in searches:
        search.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--dependency-directory", type=Path, action="append", default=[])
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    if args.worker:
        verify(args)
        return
    args.output.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([sys.executable, __file__, *sys.argv[1:], "--worker"],
                            capture_output=True, text=True, timeout=60)
    (args.output / "authority.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout + result.stderr, end="")
    result.check_returncode()


if __name__ == "__main__":
    main()
