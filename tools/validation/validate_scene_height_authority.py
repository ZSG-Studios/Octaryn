"""Exercise 3D residency and swept vertical authority through the production DLL."""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from reflection_fixture import generate
from validate_meshopt_authority import State, Input, Tick
from validate_gltf_tile_authority import Stats


def verify(args):
    directory = args.output.resolve() / "fixture"
    generate(directory)
    source = directory / "reflection.gltf"
    document = json.loads(source.read_text())
    document["meshes"][0]["primitives"] = document["meshes"][0]["primitives"][:1]
    document["nodes"] = [dict(mesh=0, translation=[0, y, 0]) for y in (0, 100, 200, 1000)]
    document["scenes"][0]["nodes"] = [0, 1, 2, 3]
    source.write_text(json.dumps(document), encoding="utf-8")
    manifest = directory / "map.json"
    manifest.write_text(json.dumps(dict(version=1, map=source.name, scene_catalog="scene.json",
                                        spawn=[0, 1.62, 6], yaw=0, pitch=0)), encoding="utf-8")
    subprocess.run([str(args.cook.resolve()), "--catalog", str(source), str(directory / "scene.json")], check=True)
    if args.ordered:
        subprocess.run([str(args.cook.resolve()), "--order", str(directory / "scene.json")], check=True)
    subprocess.run([str(args.cook.resolve()), "--cook", str(directory / "scene.json")], check=True)
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
    ready = native.octaryn_server_map_world_collision_ready_state
    ready.argtypes = [c.c_void_p, c.POINTER(State), c.POINTER(Input), c.c_double]
    stats = native.octaryn_server_map_world_collision_stats
    stats.argtypes = [c.c_void_p, c.POINTER(Stats), c.c_uint32]
    for mode in ("teleport", "falling", "vertical_flight", "flight_clamp"):
        handle = create(str(source).encode(), str(manifest).encode())
        assert handle, "3D fixture authority failed to start"
        try:
            assert count(handle) == 2, "initial residency fetched vertically distant floors"
            state, control, tick = State(), Input(), Tick()
            state.z = 6
            dt = 1 / 60
            if mode == "teleport":
                state.y = 103
            elif mode == "falling":
                state.y, state.vy, dt = 130, -140, .25
            elif mode == "vertical_flight":
                state.y, control.flags, control.my, dt = 180, 6, 1, .25
            else:
                state.y, control.flags = 1100, 4
            previous = bytes(state)
            assert step(handle, c.byref(control), dt, c.byref(state), c.byref(tick)) == 0
            assert bytes(state) == previous, mode + " advanced before its complete swept volume was ready"
            deadline = time.monotonic() + 20
            while True:
                result = ready(handle, c.byref(state), c.byref(control), dt)
                assert result >= 0 and time.monotonic() < deadline, mode + " residency failed"
                if result == 0:
                    break
                time.sleep(.001)
            assert count(handle) == 4, mode + " fetched an unrelated vertical floor or omitted its destination"
            assert step(handle, c.byref(control), dt, c.byref(state), c.byref(tick)) == 0
            if mode in ("vertical_flight", "flight_clamp"):
                assert abs(state.y - (205 if mode == "vertical_flight" else 1000)) < .01, state.y
            else:
                for _ in range(240):
                    assert step(handle, c.byref(control), 1 / 60, c.byref(state), c.byref(tick)) == 0
                assert state.grounded and abs(state.y - 101.62) < .05, (state.y, state.grounded)
            snapshot = Stats()
            assert stats(handle, c.byref(snapshot), c.sizeof(snapshot)) == 0
            assert snapshot.failed == 0 and snapshot.bytes <= snapshot.budget
        finally:
            destroy(handle)
    for search in searches:
        search.close()
    print(f"scene_height_authority=passed ordered={int(args.ordered)} same_xz_floors=4 initial_resident_floors=1 "
          "teleport_held=1 falling_sweep_held=1 vertical_flight_sweep_held=1 flight_clamp_held=1 original_budget=1", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--cook", type=Path, required=True)
    parser.add_argument("--dependency-directory", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--worker", action="store_true")
    parser.add_argument("--ordered", action="store_true")
    args = parser.parse_args()
    if args.worker:
        verify(args)
        return
    args.output.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([sys.executable, __file__, *sys.argv[1:], "--worker"],
                            capture_output=True, text=True, timeout=90)
    (args.output / "authority.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout + result.stderr, end="")
    result.check_returncode()


if __name__ == "__main__":
    main()
