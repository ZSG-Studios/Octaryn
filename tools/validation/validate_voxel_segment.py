"""Segment-query differential: production Slang CPU execution versus a Python DDA mirror.

Covers the anchor-rebased voxel_trace_segment API over hash-resident chunks:
negative/rebased anchors, boundary starts, zero direction components, known-empty
misses, hash-miss unknowns, partial-height gaps, exhausted budgets and random
in-chunk rays compared against an independent float64 Amanatides/Woo mirror.
"""
import ctypes as c
import json
import math
import random

from validate_voxel_dda import ROOT, Buffer, Dispatch, compile_shader
from validate_voxel_buffers import Header, build_hash

INF = float("inf")
STATUS = dict(invalid=0, hit=1, miss=2, unknown=3, exhausted=4)


class Case(c.Structure):
    _fields_ = [("origin_tmin", c.c_float*4), ("direction_tmax", c.c_float*4),
                ("anchor_steps", c.c_int32*4)]


class Result(c.Structure):
    _fields_ = [("voxel_status", c.c_int32*4), ("face_material", c.c_int32*4),
                ("time_exit", c.c_float*4), ("steps", c.c_uint32*4),
                ("epoch_generation", c.c_uint32*4), ("chunk", c.c_int32*4)]


class Globals(c.Structure):
    _fields_ = [("chunks", Buffer), ("leaves", Buffer), ("macros", Buffer), ("materials", Buffer),
                ("counts", c.c_uint32*4), ("hash", Buffer), ("mask", c.c_uint32),
                ("generation", c.c_uint32*2), ("pad", c.c_uint32),
                ("cases", Buffer), ("results", Buffer)]


def floor_div32(values):
    return tuple(value//32 for value in values)


def build_world(chunks):
    headers = (Header*len(chunks))()
    leaves = (c.c_uint64*(512*len(chunks)))()
    macros = (c.c_uint64*(8*len(chunks)))()
    materials = (c.c_uint32*(16384*len(chunks)))()
    for index, (coordinate, voxels, min_y, max_y, epoch) in enumerate(chunks):
        mask = 0
        for (x, y, z), material in voxels.items():
            leaf = x//4+8*(y//4+8*(z//4))
            leaves[index*512+leaf] |= 1 << (x % 4+4*(y % 4+4*(z % 4)))
            macro = x//16+2*(y//16+2*(z//16))
            macros[index*8+macro] |= 1 << ((x//4) % 4+4*((y//4) % 4+4*((z//4) % 4)))
            mask |= 1 << macro
            linear = x+32*(y+32*z)
            materials[index*16384+linear//2] |= material << ((linear % 2)*16)
        headers[index] = Header(tuple(coordinate), mask, tuple(epoch), index*512, index*8,
                                index*16384, min_y, max_y, 1)
    table, table_mask = build_hash([tuple(chunk[0]) for chunk in chunks])
    counts = (len(chunks), 512*len(chunks), 8*len(chunks), 16384*len(chunks))
    return headers, leaves, macros, materials, table, table_mask, counts


def normalize(vector):
    length = math.sqrt(sum(value*value for value in vector))
    return [value/length for value in vector]


def slab_clip(origin, direction, lower, upper, tmin, tmax):
    begin, end, normal = tmin, tmax, (0, 0, 0)
    for axis in range(3):
        if direction[axis] == 0:
            if origin[axis] < lower[axis] or origin[axis] >= upper[axis]:
                return None
            continue
        a = (lower[axis]-origin[axis])/direction[axis]
        b = (upper[axis]-origin[axis])/direction[axis]
        near, far = min(a, b), max(a, b)
        if near > begin:
            begin = near
            axis_normal = [0, 0, 0]
            axis_normal[axis] = -1 if direction[axis] > 0 else 1
            normal = tuple(axis_normal)
        end = min(end, far)
        if begin >= end:
            return None
    return begin, end, normal


def dda_cells(origin, direction, begin, end, size, lower, upper, clip_normal):
    """Mirror of VoxelDda.slang: half-open cells, exact boundaries, X/Y/Z ties."""
    cell, step, next_times = [0]*3, [0]*3, [INF]*3
    for axis in range(3):
        step[axis] = 1 if direction[axis] > 0 else (-1 if direction[axis] < 0 else 0)
        coordinate = (origin[axis]+direction[axis]*begin)/size
        index = math.floor(coordinate)
        if direction[axis] < 0 and coordinate == index:
            index -= 1
        cell[axis] = min(max(index*size, lower[axis]), upper[axis]-size)

    def boundaries():
        for axis in range(3):
            if step[axis] == 0:
                next_times[axis] = INF
            else:
                boundary = cell[axis]+(size if step[axis] > 0 else 0)
                next_times[axis] = (float(boundary)-origin[axis])/direction[axis]
    boundaries()
    time, normal = begin, clip_normal
    while True:
        exit_time = min(end, *next_times)
        if time < exit_time:
            yield tuple(cell), time, exit_time, normal
        crossing = min(next_times)
        if crossing >= end or not math.isfinite(crossing):
            return
        moved, step_normal = False, [0, 0, 0]
        for axis in range(3):
            if next_times[axis] == crossing and step[axis] != 0:
                cell[axis] += step[axis]*size
                if not moved:
                    step_normal[axis] = -step[axis]
                    moved = True
        if not moved:
            return
        time, normal = max(time, crossing), tuple(step_normal)
        boundaries()


def first_occupied(origin, direction, tmin, tmax, voxels, lower, upper):
    interval = slab_clip(origin, direction, [float(value) for value in lower],
                         [float(value) for value in upper], tmin, tmax)
    if interval is None or interval[0] > tmin:
        return None
    for cell, enter, exit_time, normal in dda_cells(origin, direction, interval[0],
                                                    interval[1], 1, lower, upper, interval[2]):
        material = voxels.get(cell)
        if material is not None:
            return cell, material, enter, exit_time, normal
    return None


PATTERN = {(8, 8, 8): 101, (20, 4, 12): 202, (0, 0, 0): 303, (31, 31, 31): 404}
FILL_OFFSET = (32, 0, 0)  # Random-fill chunk sits at K+(1,0,0), one chunk east.
PATTERN_EPOCH = (0xAABBCCDD, 0x11223344)
RANDOM_EPOCH = (77, 88)
GENERATION = (0x0BADF00D, 0xFEEDFACE)
ANCHORS = [(0, 0, 0), (2000000000, -2000000000, 16777217), (-16777217, 16777217, -100)]


def fixture():
    rng = random.Random(97)
    fill = {}
    while len(fill) < 300:
        fill[(rng.randrange(32), rng.randrange(32), rng.randrange(32))] = rng.randrange(1, 60000)
    chunks = []
    for anchor in ANCHORS:
        base = floor_div32(anchor)
        chunks += [(base, PATTERN, 0, 32, PATTERN_EPOCH),
                   ((base[0]-1, base[1], base[2]), {}, 0, 32, (0, 0)),
                   ((base[0], base[1], base[2]-1), {}, 8, 32, (0, 0)),
                   ((base[0]+1, base[1], base[2]), fill, 0, 32, RANDOM_EPOCH)]
    return chunks, fill


def hand_cases(base, rebase):
    """(local origin, direction, tmin, tmax, steps, expected dict) per anchor frame."""
    hit = lambda voxel, material, t, face, epoch=PATTERN_EPOCH: dict(  # noqa: E731
        status=STATUS["hit"], voxel=tuple(b+v for b, v in zip(base, voxel)),
        material=material, t=t, face=face, epoch=epoch)
    rows = [
        ((8.5, 8.5, 3), (0, 0, 1), 0, 20, 65536, hit((8, 8, 8), 101, 5, (0, 0, -1))),
        ((8.5, 8.5, 8.5), (1, 0, 0), 0, 5, 65536, hit((8, 8, 8), 101, 0, (0, 0, 0))),
        ((8.5, 8.5, 8), (0, 0, -1), 0, 60, 65536, dict(status=STATUS["unknown"], t=40)),
        ((8.5, 4, 8), (0, 0, -1), 0, 60, 65536, dict(status=STATUS["unknown"], t=8)),
        ((0.5, 8.5, 8.5), (-1, 0, 0), 0, 20, 65536, dict(status=STATUS["miss"], t=20)),
        ((0.5, 8.5, 8.5), (-1, 0, 0), 0, 40, 65536, dict(status=STATUS["unknown"], t=32.5)),
        ((8.5, 8.5, 3), (0, 0, 1), 0, 20, 1, dict(status=STATUS["exhausted"])),
        ((8.5, 8.5, 3), (0, 0, 1), 5, 5, 65536, dict(status=STATUS["invalid"])),
        ((8.5, 8.5, 3), (0, 0, 0), 0, 20, 65536, dict(status=STATUS["invalid"])),
        ((8.5, 8.5, 3), (0, 0, 1), 10, 20, 65536, dict(status=STATUS["miss"], t=20)),
        ((7.5, 7.5, 7.5), (1, 1, 1), 0, 5, 65536,
         hit((8, 8, 8), 101, 0.5*math.sqrt(3), (-1, 0, 0))),
        ((3, 4.5, 12.5), (1, 0, 0), 0, 30, 65536, hit((20, 4, 12), 202, 17, (-1, 0, 0))),
        ((-0.5, 8.5, 8.5), (1, 0, 0), 0, 20, 65536, hit((8, 8, 8), 101, 8.5, (-1, 0, 0))),
        ((0.5, 0.5, 0.5), (-1, -1, -1), 0, 5, 65536, hit((0, 0, 0), 303, 0, (0, 0, 0))),
        ((31.5, 31.5, 31.5), (1, 1, 1), 0, 5, 65536, hit((31, 31, 31), 404, 0, (0, 0, 0))),
        ((8.5, 30.5, 8.5), (0, 1, 0), 0, 10, 65536, dict(status=STATUS["unknown"], t=1.5)),
        ((0, 8.5, 8.5), (-1, 0, 0), 0, 10, 65536, dict(status=STATUS["miss"], t=10)),
        ((0, 8.5, 8.5), (1, 0, 0), 0, 20, 65536, hit((8, 8, 8), 101, 8, (-1, 0, 0))),
    ]
    return [([point[axis]-rebase[axis] for axis in range(3)], direction, tmin, tmax, steps, expect)
            for point, direction, tmin, tmax, steps, expect in rows]


def quantize(value):
    return c.c_float(value).value


def random_cases(fill, rebase, rng):
    rows = []
    for _ in range(150):
        origin = [rng.uniform(0, 32) for _ in range(3)]
        # Keep float32 anchor rebasing away from exact cell-boundary flips.
        if any(min(value % 1, 1-value % 1) < 1e-4 for value in origin):
            continue
        origin = [quantize(value) for value in origin]
        direction = [0., 0., 0.]
        while not any(direction):
            direction = normalize([rng.uniform(-1, 1) for _ in range(3)])
        box = slab_clip(origin, direction, [0.]*3, [32.]*3, 0, INF)
        tmax = quantize(box[1]*rng.uniform(.3, .9))
        tmin = quantize(tmax*rng.uniform(0, .3))
        expected = first_occupied(origin, direction, tmin, tmax, fill, (0,)*3, (32,)*3)
        rows.append(([origin[axis]+FILL_OFFSET[axis]-rebase[axis] for axis in range(3)],
                     direction, tmin, tmax, 65536, expected))
    return rows


def run():
    chunks, fill = fixture()
    headers, leaves, macros, materials, table, table_mask, counts = build_world(chunks)
    library = compile_shader(ROOT / "tools/validation/VoxelSegmentProbe.slang", "Segment")
    entry = library.main_0
    entry.argtypes = [c.POINTER(Dispatch), c.c_void_p, c.POINTER(Globals)]
    rng = random.Random(1997)
    rows, expected = [], []
    for anchor_index, anchor in enumerate(ANCHORS):
        base = tuple(value*32 for value in floor_div32(anchor))
        rebase = tuple(anchor[axis]-base[axis] for axis in range(3))
        for origin, direction, tmin, tmax, steps, expect in hand_cases(base, rebase):
            rows.append(Case((*origin, tmin), (*direction, tmax), (*anchor, steps)))
            expected.append(expect)
        for origin, direction, tmin, tmax, steps, mirror in random_cases(fill, rebase, rng):
            rows.append(Case((*origin, tmin), (*direction, tmax), (*anchor, steps)))
            if mirror is None:
                expected.append(dict(status=STATUS["miss"], t=tmax))
            else:
                cell, material, enter, exit_time, normal = mirror
                expected.append(dict(status=STATUS["hit"],
                                     voxel=tuple(b+v+o for b, v, o in zip(base, cell, FILL_OFFSET)),
                                     material=material, t=enter, exit=exit_time,
                                     face=normal, epoch=RANDOM_EPOCH))
    inputs, outputs = (Case*len(rows))(*rows), (Result*len(rows))()
    globals_ = Globals(Buffer(c.addressof(headers), len(headers)),
                       Buffer(c.addressof(leaves), len(leaves)),
                       Buffer(c.addressof(macros), len(macros)),
                       Buffer(c.addressof(materials), len(materials)), counts,
                       Buffer(c.addressof(table), len(table)), table_mask, GENERATION, 0,
                       Buffer(c.addressof(inputs), len(rows)), Buffer(c.addressof(outputs), len(rows)))
    entry(c.byref(Dispatch((0, 0, 0), (len(rows), 1, 1))), None, c.byref(globals_))
    hits = 0
    for index, (actual, want) in enumerate(zip(outputs, expected)):
        status = actual.voxel_status[3]
        assert status == want["status"], f"Status {index}: {status} != {want['status']}"
        assert list(actual.epoch_generation[2:]) == list(GENERATION), f"Generation {index}"
        assert sum(actual.steps) <= rows[index].anchor_steps[3], f"Budget {index}"
        if status != STATUS["hit"]:
            if "t" in want:
                assert abs(actual.time_exit[0]-want["t"]) <= max(2e-4, abs(want["t"])*2e-6), \
                    f"Distance {index}: {actual.time_exit[0]} != {want['t']}"
            continue
        hits += 1
        assert list(actual.voxel_status[:3]) == list(want["voxel"]), f"Voxel {index}"
        assert actual.face_material[3] == want["material"], f"Material {index}"
        assert list(actual.face_material[:3]) == list(want["face"]), f"Face {index}"
        assert list(actual.epoch_generation[:2]) == list(want["epoch"]), f"Epoch {index}"
        assert abs(actual.time_exit[0]-want["t"]) <= max(2e-4, abs(want["t"])*2e-6), \
            f"Distance {index}: {actual.time_exit[0]} != {want['t']}"
        if "exit" in want:
            assert abs(actual.time_exit[1]-want["exit"]) <= max(2e-4, abs(want["exit"])*2e-6), \
                f"Exit {index}: {actual.time_exit[1]} != {want['exit']}"
    report = dict(status="passed", cases=len(rows), hits=hits, anchors=ANCHORS,
                  chunk_lookup="hash linear probing", statuses=["hit", "miss", "unknown",
                  "exhausted", "invalid"], production_slang_cpu=True,
                  targets=["SPIR-V", "DXIL", "MSL"], gpu_runtime=False)
    log = ROOT / "logs/tools/voxel-segment-query.json"
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(json.dumps(report, indent=2))
    print(json.dumps(report))


if __name__ == "__main__":
    run()
