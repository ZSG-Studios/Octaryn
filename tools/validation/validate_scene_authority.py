"""Compare source and scene-catalog manifests through the same native authority."""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from validate_meshopt_authority import State, Input, Tick, Hit
from validate_gltf_tile_authority import Stats


def verify(args):
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
    spawn = native.octaryn_server_map_world_spawn
    spawn.argtypes = [c.c_void_p, c.POINTER(State)]
    step = native.octaryn_server_map_world_step
    step.argtypes = [c.c_void_p, c.POINTER(Input), c.c_double, c.POINTER(State), c.POINTER(Tick)]
    ray = native.octaryn_server_map_world_raycast
    ray.argtypes = [c.c_void_p] + [c.c_float] * 7 + [c.POINTER(Hit)]
    ready = native.octaryn_server_map_world_collision_ready_at
    ready.argtypes = [c.c_void_p] + [c.c_float] * 4
    stats = native.octaryn_server_map_world_collision_stats
    stats.argtypes = [c.c_void_p, c.POINTER(Stats), c.c_uint32]
    evidence = []
    for path in (args.directory / "source-map.json", args.directory / "map.json"):
        manifest = path.resolve()
        source = manifest.parent / json.loads(manifest.read_text(encoding="utf-8"))["map"]
        handle = create(str(source).encode(), str(manifest).encode())
        assert handle, "authority rejected original source of the instanced scene"
        observations = []
        try:
            assert 0 < count(handle) <= 20, "authority residency lost or expanded source triangles"
            for x in (0, 16):
                deadline = time.monotonic() + 20
                while True:
                    result = ready(handle, x, 3, 6, 11)
                    assert result >= 0 and time.monotonic() < deadline, "protected scene collision failed or timed out"
                    if result == 0:
                        break
                    time.sleep(.001)
                hit = Hit()
                assert ray(handle, x, 5, 6, 0, -1, 0, 10, c.byref(hit)) == 0
                assert hit.hit and abs(hit.y) < 1e-5 and abs(hit.distance - 5) < 1e-5
                state, control, tick = State(), Input(), Tick()
                assert spawn(handle, c.byref(state)) == 0
                state.x, state.y, state.z = x, 3, 6
                for _ in range(240):
                    assert step(handle, c.byref(control), 1 / 60, c.byref(state), c.byref(tick)) == 0
                assert state.grounded and abs(state.y - 1.62) < .05, (state.y, state.grounded)
                observations.append((hit.x, hit.y, hit.z, state.x, state.y, state.z, state.grounded))
            assert count(handle) == 20, "combined protected neighborhoods lost a source instance"
            if path.name == "map.json":
                time.sleep(2.1)
                assert ready(handle, 100, 3, 6, 3) == 0
                snapshot = Stats()
                assert stats(handle, c.byref(snapshot), c.sizeof(snapshot)) == 0
                assert snapshot.evictions and snapshot.resident == 0, "scene collision did not deactivate distant neighborhoods"
                state, control, tick = State(), Input(), Tick()
                assert spawn(handle, c.byref(state)) == 0
                state.y = 3
                previous = bytes(state)
                assert step(handle, c.byref(control), 1 / 60, c.byref(state), c.byref(tick)) == 0
                assert bytes(state) == previous, "authority advanced movement before protected collision was ready"
                deadline = time.monotonic() + 20
                while ready(handle, 0, 3, 6, 11) == 1:
                    assert time.monotonic() < deadline, "scene collision reactivation timed out"
                    time.sleep(.001)
                assert ready(handle, 0, 3, 6, 11) == 0 and count(handle) > 0
        finally:
            destroy(handle)
        evidence.append(observations)
    assert evidence[0] == evidence[1], "rendering catalog changed authoritative collision"
    print("scene_authority=passed manifests=2 triangles=20 instances=2 mirrored=1 grounded=4 rays=4 "
          "collision_parity=1 eviction=1 reactivation=1 pending_movement_held=1", flush=True)
    for search in searches:
        search.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--dependency-directory", type=Path, action="append", default=[])
    parser.add_argument("--directory", type=Path, required=True)
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
