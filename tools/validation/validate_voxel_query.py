"""HDDA query regression: actual Slang CPU execution against independent ray/box intersections."""
import ctypes as c
import json
import math
import random

from validate_voxel_dda import ROOT, Buffer, Dispatch, Globals, compile_shader


class Case(c.Structure):
    _fields_ = [("origin_chunk", c.c_int32*4), ("origin", c.c_float*4),
                ("direction", c.c_float*4), ("lower", c.c_int32*4), ("upper", c.c_int32*4),
                ("occupied_chunk", c.c_int32*4), ("voxel", c.c_int32*4),
                ("unknown", c.c_int32*4), ("options", c.c_uint32*4)]


class Result(c.Structure):
    _fields_ = [("chunk_status", c.c_int32*4), ("voxel_material", c.c_int32*4),
                ("distance_normal", c.c_float*4), ("exit", c.c_float*4), ("steps", c.c_uint32*4),
                ("epoch", c.c_uint32*4)]


def make(origin, direction, target, anchor=(0, 0, 0), tmin=0, tmax=1000, budget=20000,
         lower=(-32,)*3, upper=(32,)*3, unknown=None, material=37):
    length = math.sqrt(sum(value*value for value in direction))
    direction = [value/length for value in direction]
    chunk = [target[i]//32+anchor[i] for i in range(3)]
    voxel = [target[i]%32 for i in range(3)]
    return Case((*anchor, 0), (*origin, tmin), (*direction, tmax),
                (*(lower[i]+anchor[i] for i in range(3)), 0),
                (*(upper[i]+anchor[i] for i in range(3)), 0), (*chunk, 0), (*voxel, 0),
                (*(unknown if unknown is not None else (0, 0, 0)), 0), (material, budget, unknown is not None, 0))


def box_interval(case, lower, upper):
    start, end = float(case.origin[3]), float(case.direction[3])
    for axis in range(3):
        origin, direction = case.origin[axis], case.direction[axis]
        if direction == 0:
            if not lower[axis] <= origin < upper[axis]:
                return None
        else:
            a, b = sorted(((lower[axis]-origin)/direction, (upper[axis]-origin)/direction))
            start, end = max(start, a), min(end, b)
    return (start, end) if start < end else None


def oracle(case):
    lower = [(case.occupied_chunk[i]-case.origin_chunk[i])*32+case.voxel[i] for i in range(3)]
    return box_interval(case, lower, [value+1 for value in lower])


def run():
    library = compile_shader(ROOT / "tools/validation/VoxelQueryProbe.slang", "Query")
    rows, expected = [], []
    def add(case, status=None):
        rows.append(case); expected.append(status if status is not None else (1 if oracle(case) else 0))
    anchors = [(0, 0, 0), (2000000000, -2000000000, 16777217), (-16777217, 16777217, -100)]
    for anchor in anchors:
        for axis in range(3):
            for sign in (-1, 1):
                direction = [0., -0., 0.];direction[axis]=sign
                for coordinate in (-33, -32, -9, -8, -5, -4, -1, 0, 3, 4, 7, 8, 31, 32, 63):
                    target = [0, 0, 0];target[axis]=coordinate
                    add(make((0, 0, 0), direction, target, anchor))
        add(make((.5, .5, .5), (1, 0, 0), (0, 0, 0), anchor, tmin=.125))
        add(make((0, 0, 0), (1, 1, 1), (4, 4, 4), anchor))
        add(make((0, 0, 0), (1, 1, 1), (4, 3, 4), anchor))
    # Long empty-space skipping, exact finite end, unknown versus known-empty,
    # exhausted budget, and outside-window segments must never become sky misses.
    add(make((.5, .5, .5), (1, 0, 0), (16383, 0, 0), tmax=17000, upper=(600, 32, 32)))
    add(make((.5, .5, .5), (1, 0, 0), (31, 0, 0), tmax=30.5))
    add(make((.5, .5, .5), (1, 0, 0), (63, 0, 0), unknown=(1, 0, 0)), 2)
    add(make((.5, .5, .5), (1, 0, 0), (31, 0, 0), budget=1), 3)
    add(make((.5, .5, .5), (1, 0, 0), (0, 0, 0), material=0, tmax=2000), 2)
    add(make((2000, .5, .5), (-1, 0, 0), (0, 0, 0), tmax=2500), 2)
    rng = random.Random(1997)
    for _ in range(1500):
        target = [rng.randrange(-200, 200) for _ in range(3)]
        origin = [rng.uniform(-31, 31) for _ in range(3)]
        center = [value+rng.uniform(.1, .9) for value in target]
        direction = [center[i]-origin[i] for i in range(3)]
        length = math.sqrt(sum(value*value for value in direction))
        add(make(origin, direction, target, rng.choice(anchors), tmax=rng.choice((length*.9, length+1))))
    inputs, outputs = (Case*len(rows))(*rows), (Result*len(rows))()
    globals_ = Globals(Buffer(c.addressof(inputs), len(rows)), Buffer(c.addressof(outputs), len(rows)))
    entry = library.main_0;entry.argtypes = [c.POINTER(Dispatch), c.c_void_p, c.POINTER(Globals)]
    entry(c.byref(Dispatch((0, 0, 0), (len(rows), 1, 1))), None, c.byref(globals_))
    for index, (case, result, status) in enumerate(zip(rows, outputs, expected)):
        assert result.chunk_status[3] == status, f"Status mismatch {index}: {result.chunk_status[3]} != {status}"
        assert sum(result.steps) <= case.options[1], f"Budget exceeded {index}"
        if status == 1:
            assert list(result.chunk_status[:3]) == list(case.occupied_chunk[:3]), f"Global chunk mismatch {index}"
            assert list(result.voxel_material[:3]) == list(case.voxel[:3]), f"Local voxel mismatch {index}"
            assert result.voxel_material[3] == case.options[0], f"Material mismatch {index}"
            assert list(result.epoch[:2]) == [9, 1], f"Epoch mismatch {index}"
            distance, end = oracle(case)
            assert abs(result.distance_normal[0]-distance) <= max(.0002, distance*2e-6), f"Distance mismatch {index}"
            assert abs(result.exit[0]-end) <= max(.0002, end*2e-6), f"Exit mismatch {index}"
    report = dict(status="passed", cases=len(rows), rebased_global_chunks=anchors, production_slang_cpu=True,
                  levels=[32, 16, 4, 1], portable_masks="uint2", targets=["SPIR-V", "DXIL", "MSL"], gpu_runtime=False)
    log = ROOT / "logs/tools/voxel-query-boundaries.json";log.write_text(json.dumps(report, indent=2))
    print(json.dumps(report))


if __name__ == "__main__":
    run()
