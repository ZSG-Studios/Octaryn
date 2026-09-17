"""Execute the production Slang DDA via Slang's C++ target and independent box oracles."""
import ctypes as c
import json
import math
from pathlib import Path
import random
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


class Case(c.Structure):
    _fields_ = [("origin", c.c_float * 4), ("direction", c.c_float * 4),
                ("lower", c.c_int32 * 4), ("upper", c.c_int32 * 4), ("occupied", c.c_int32 * 4)]


class Result(c.Structure):
    _fields_ = [("distance_normal", c.c_float * 4), ("cell_status", c.c_int32 * 4),
                ("counts", c.c_uint32 * 4)]


class Buffer(c.Structure):
    _fields_ = [("data", c.c_void_p), ("count", c.c_size_t)]


class Globals(c.Structure):
    _fields_ = [("cases", Buffer), ("results", Buffer)]


class Dispatch(c.Structure):
    _fields_ = [("start", c.c_uint32 * 3), ("end", c.c_uint32 * 3)]


def oracle(case):
    start, end = float(case.origin[3]), float(case.direction[3])
    if not math.isfinite(start + end) or start < 0 or start >= end:
        return None
    if not any(case.direction[:3]):
        return None
    for axis in range(3):
        origin, direction = float(case.origin[axis]), float(case.direction[axis])
        lower, upper = case.occupied[axis], case.occupied[axis] + case.lower[3]
        if lower < case.lower[axis] or upper > case.upper[axis]:
            return None
        if not direction:
            if not lower <= origin < upper:
                return None
        else:
            near, far = sorted(((lower-origin)/direction, (upper-origin)/direction))
            start, end = max(start, near), min(end, far)
    return start if start < end else None


def make(origin, direction, occupied, size=1, tmin=0, tmax=10000, lower=(-64,)*3, upper=(64,)*3):
    return Case((*origin, tmin), (*direction, tmax), (*lower, size), (*upper, 0), (*occupied, 0))


def cases():
    rows = []
    for size in (1, 4, 8, 32):
        for axis in range(3):
            for sign in (-1, 1):
                direction = [0., -0., 0.]; direction[axis] = sign
                origin = [.25, .25, .25]; origin[axis] = 0
                target = [0, 0, 0]; target[axis] = -size if sign < 0 else 0
                rows += [make(origin, direction, target, size),
                         make(origin, direction, target, size, tmin=.125),
                         make(origin, direction, target, size, tmin=size, tmax=size+1)]
                target[axis] = sign*size*2 if sign > 0 else -size*3
                rows += [make(origin, direction, target, size, tmax=size*2),
                         make(origin, direction, target, size, tmax=size*2+.25)]
        rows += [make((0, 0, 0), (1, 1, 1), (size, size, size), size),
                 make((0, 0, 0), (1, 1, 1), (size, 0, 0), size),
                 make((0, 0, 0), (-1, -1, -1), (-size, -size, -size), size),
                 make((0, 0, 0), (0, 0, 0), (0, 0, 0), size)]
    rows += [make((-8192., .5, .5), (1, 0, 0), (8191, 0, 0), tmax=20000,
                  lower=(-8192, -32, -32), upper=(8192, 32, 32)),
             make((64, .5, .5), (0, 1, 0), (63, 0, 0)),
             make((64, .5, .5), (-1, 0, 0), (63, 0, 0)),
             make((.5, .5, .5), (1, 0, 0), (0, 0, 0), tmax=0)]
    rng = random.Random(1741)
    for _ in range(2000):
        size = rng.choice((1, 4, 8, 32))
        occupied = [rng.randrange(-64//size, 64//size)*size for _ in range(3)]
        origin = [rng.uniform(-100, 100) for _ in range(3)]
        target = [value + rng.random()*size for value in occupied]
        direction = [target[i]-origin[i] for i in range(3)]
        length = math.sqrt(sum(value*value for value in direction))
        direction = [value/length for value in direction]
        rows.append(make(origin, direction, occupied, size, tmax=rng.choice((length*.9, length+1))))
    return rows


def compile_shader(shader, stem):
    output = ROOT / "build/release-windows/tools/voxel-dda"
    output.mkdir(parents=True, exist_ok=True)
    slang = ROOT / "build/dependencies/slang-2026.17.1/bin/slangc.exe"
    for target, suffix, extra in (("cpp", "cpp", []), ("spirv", "spv", ["-profile", "spirv_1_5"]),
                                  ("dxil", "dxil", ["-profile", "sm_6_0"]), ("metal", "metal", [])):
        if target == "dxil":
            extra += ["-dxc-path", str(ROOT / "build/dependencies/slang-rhi-windows-x64-Release/_deps/dxc-src/bin/x64")]
        subprocess.run([str(slang), str(shader), "-entry", "main", "-target", target,
                        *extra, "-o", str(output / (stem+"."+suffix))], check=True)
    vs = find_vs_root(); import_vs_environment(vs, "x64"); prepend_tool_dirs(ROOT, vs, "x64")
    subprocess.run(["clang-cl", "/nologo", "/LD", "/O2", "/EHsc", stem+".cpp", "/Fe:"+stem+".dll"],
                   cwd=output, check=True)
    return c.CDLL(str(output / (stem+".dll")))


def run():
    library = compile_shader(ROOT / "tools/validation/VoxelDdaProbe.slang", "Probe")
    entry = library.main_0
    entry.argtypes = [c.POINTER(Dispatch), c.c_void_p, c.POINTER(Globals)]
    rows = cases(); inputs = (Case*len(rows))(*rows); outputs = (Result*len(rows))()
    globals_ = Globals(Buffer(c.addressof(inputs), len(rows)), Buffer(c.addressof(outputs), len(rows)))
    entry(c.byref(Dispatch((0, 0, 0), (len(rows), 1, 1))), None, c.byref(globals_))
    hits = 0
    for index, (case, actual) in enumerate(zip(rows, outputs)):
        expected = oracle(case)
        assert actual.cell_status[3] != 3, f"Traversal exhausted: case {index}"
        assert (actual.cell_status[3] == 1) == (expected is not None), f"Hit mismatch: case {index}"
        if expected is not None:
            hits += 1
            assert list(actual.cell_status[:3]) == list(case.occupied[:3]), f"Cell mismatch: {index}"
            assert abs(actual.distance_normal[0]-expected) <= max(.0001, abs(expected)*2e-6), f"Distance mismatch: {index}"
    report = dict(status="passed", cases=len(rows), hits=hits, production_slang_cpu=True,
                  targets=["SPIR-V", "DXIL", "MSL"], gpu_runtime=False)
    log = ROOT / "logs/tools/voxel-dda-boundaries.json"; log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(json.dumps(report, indent=2))
    print(json.dumps(report))


if __name__ == "__main__":
    run()
