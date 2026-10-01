"""Check authoritative collision from the production meshopt import fixture."""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


class State(c.Structure):
    _fields_ = [(name, c.c_float) for name in
                ("x", "y", "z", "pitch", "yaw", "vx", "vy", "vz")]
    _fields_ += [("grounded", c.c_uint32), ("mode", c.c_uint32), ("jump", c.c_uint16)]


class Input(c.Structure):
    _fields_ = [("flags", c.c_uint32), ("controller", c.c_uint32)]
    _fields_ += [(name, c.c_float) for name in
                ("mx", "my", "mz", "cx", "cy", "cz", "pitch", "yaw")]
    _fields_ += [("mouse", c.c_int32)]


class Tick(c.Structure):
    _fields_ = [("intent", c.c_uint32), ("reserved", c.c_uint32)]
    _fields_ += [(name, c.c_float) for name in ("dx", "dy", "dz")]


class Hit(c.Structure):
    _fields_ = [("hit", c.c_uint32), ("material", c.c_uint32)]
    _fields_ += [(name, c.c_float) for name in ("x", "y", "z", "nx", "ny", "nz", "distance")]
    _fields_ += [("triangle", c.c_uint32)]


def verify(args):
    assert (c.sizeof(State), c.sizeof(Input), c.sizeof(Tick), c.sizeof(Hit)) == (44, 44, 20, 40)
    library = args.library.resolve()
    search = os.add_dll_directory(str(library.parent)) if os.name == "nt" else None
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
    fixture = args.fixture.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = args.output.resolve() / "collision-map.json"
    manifest.write_text(json.dumps({"version": 1, "map": str(fixture),
                                    "spawn": [0, 1.62, 0], "yaw": 0, "pitch": 0}), encoding="utf-8")
    handle = create(str(fixture).encode(), str(manifest).encode())
    assert handle, "server failed to load the compressed map"
    try:
        assert count(handle) == 4, "server flattened the wrong triangles or lost repeated instances"
        for x in (0, 24):
            hit = Hit()
            assert ray(handle, x, 5, 0, 0, -1, 0, 10, c.byref(hit)) == 0
            assert hit.hit and abs(hit.y) < 1e-5 and abs(hit.distance - 5) < 1e-5
            state, control, tick = State(), Input(), Tick()
            assert spawn(handle, c.byref(state)) == 0
            state.x, state.y = x, 3
            for _ in range(240):
                assert step(handle, c.byref(control), 1 / 60, c.byref(state), c.byref(tick)) == 0
            assert state.grounded and abs(state.y - 1.62) < .05, (state.y, state.grounded)
        miss = Hit()
        assert ray(handle, 100, 5, 0, 0, -1, 0, 10, c.byref(miss)) == 1
    finally:
        destroy(handle)
    broken = args.output.resolve() / "corrupt"
    broken.mkdir(exist_ok=True)
    shutil.copyfile(fixture, broken / fixture.name)
    binary = fixture.parent / "geometry data.bin"
    data = bytearray(binary.read_bytes())
    data[0] = 0
    (broken / binary.name).write_bytes(data)
    assert not create(str(broken / fixture.name).encode(), str(manifest).encode()), "server accepted corrupt meshopt"
    if args.large_source:
        assert not create(str(args.large_source.resolve()).encode(), str(manifest).encode()), "large source bypassed collision budget"
    print("meshopt_authority=passed triangles=4 instances=2 grounded=2 raycast=1 corrupt_rejected=1 "
          f"large_scene_bounded={int(bool(args.large_source))}", flush=True)
    if search:
        search.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--large-source", type=Path)
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
