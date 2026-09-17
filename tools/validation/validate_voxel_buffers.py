"""Exercise the agreed 48-byte CPU/GPU voxel payload through actual compiled Slang."""
import ctypes as c
import json

from validate_voxel_dda import ROOT, Buffer, Dispatch, compile_shader
from validate_voxel_query import Case, Result, make


class Header(c.Structure):
    _fields_ = [("coordinate", c.c_int32*3), ("macro_mask", c.c_uint32),
                ("epoch", c.c_uint32*2), ("leaf_offset", c.c_uint32), ("macro_offset", c.c_uint32),
                ("material_offset", c.c_uint32), ("min_y", c.c_uint32), ("max_y", c.c_uint32), ("flags", c.c_uint32)]


class Globals(c.Structure):
    _fields_ = [("chunks", Buffer), ("leaves", Buffer), ("macros", Buffer), ("materials", Buffer),
                ("counts", c.c_uint32*4), ("hash", Buffer), ("mask", c.c_uint32),
                ("generation", c.c_uint32*2), ("pad", c.c_uint32),
                ("cases", Buffer), ("results", Buffer)]


MASK32 = 0xFFFFFFFF


def trace_chunk_hash(x, y, z):
    """Mirror of trace_chunk_hash (VoxelTraceUploadPlan.h) for fixture tables."""
    h = ((x & MASK32)*0x8DA6B343 ^ (y & MASK32)*0xD8163841 ^ (z & MASK32)*0xCB1AB31F) & MASK32
    h ^= h >> 16;h = (h*0x7FEB352D) & MASK32
    h ^= h >> 15;h = (h*0x846CA68B) & MASK32
    return (h ^ (h >> 16)) & MASK32


def build_hash(keys):
    count = 2
    while count < len(keys)*2:
        count *= 2
    table = (c.c_uint32*count)()
    for index, key in enumerate(keys):
        at = trace_chunk_hash(*key) & (count-1)
        while table[at]:
            at = (at+1) & (count-1)
        table[at] = index+1
    return table, count-1


def run():
    assert c.sizeof(Header) == 48 and Header.epoch.offset == 16 and Header.flags.offset == 44
    assert c.sizeof(Globals) == 144, c.sizeof(Globals)
    library = compile_shader(ROOT / "tools/validation/VoxelBufferProbe.slang", "Buffers")
    entry = library.main_0;entry.argtypes = [c.POINTER(Dispatch), c.c_void_p, c.POINTER(Globals)]
    masks = (c.c_uint64*1024)();macros = (c.c_uint64*16)();materials = (c.c_uint32*32768)()
    headers = (Header*2)(Header((0, 0, 0), 255, (0x12345678, 0x9abcdef0), 3, 2, 5, 0, 32, 1),
                         Header((-7, 3, -9), 0, (0, 0), 512, 8, 16384, 0, 32, 1))
    hash_table, hash_mask = build_hash([tuple(headers[i].coordinate) for i in range(2)])
    rows, expected = [], []
    for brick in range(512):
        bx, by, bz = brick%8, (brick//8)%8, brick//64
        bit = brick%64;x, y, z = bx*4+bit%4, by*4+(bit//4)%4, bz*4+bit//16
        masks[3+brick] = 1 << bit
        macro = bx//4+2*(by//4+2*(bz//4));macro_bit = bx%4+4*(by%4+4*(bz%4))
        macros[2+macro] |= 1 << macro_bit
        voxel = x+32*(y+32*z);material = brick+1000
        materials[5+voxel//2] |= material << ((voxel%2)*16)
        rows.append(make((x+.25, y+.25, z+.25), (1, 0, 0), (x, y, z), tmax=.5, lower=(0,)*3, upper=(1,)*3))
        expected.append(material)
    def execute(rows):
        inputs, outputs = (Case*len(rows))(*rows), (Result*len(rows))()
        globals_ = Globals(Buffer(c.addressof(headers), len(headers)), Buffer(c.addressof(masks), len(masks)),
                           Buffer(c.addressof(macros), len(macros)), Buffer(c.addressof(materials), len(materials)),
                           (len(headers), len(masks), len(macros), len(materials)),
                           Buffer(c.addressof(hash_table), len(hash_table)), hash_mask,
                           (0x11223344, 0x55667788), 0,
                           Buffer(c.addressof(inputs), len(rows)), Buffer(c.addressof(outputs), len(rows)))
        entry(c.byref(Dispatch((0, 0, 0), (len(rows), 1, 1))), None, c.byref(globals_))
        return outputs
    outputs = execute(rows)
    for index, (hit, case, material) in enumerate(zip(outputs, rows, expected)):
        assert hit.chunk_status[3] == 1, f"Candidate missing {index}"
        assert list(hit.voxel_material[:3]) == list(case.voxel[:3]), f"Voxel decode {index}"
        assert hit.voxel_material[3] == material, f"Packed material decode {index}"
        assert list(hit.epoch[:2]) == [0x12345678, 0x9abcdef0], f"Epoch words {index}"
        assert list(hit.epoch[2:]) == [0x11223344, 0x55667788], f"Generation uniform {index}"
    # Hash-resident negative-coordinate known-empty chunk: a miss, not unknown.
    empty = make((1.5, 10.5, 1.5), (1, 0, 0), (0, 0, 0), anchor=(-7, 3, -9), tmax=2,
                 lower=(-1, -1, -1), upper=(2, 2, 2))
    assert execute([empty])[0].chunk_status[3] == 0
    headers[0].macro_mask = 0
    known = make((1.5, 10.5, 1.5), (1, 0, 0), (0, 0, 0), tmax=2, lower=(0,)*3, upper=(1,)*3)
    assert execute([known])[0].chunk_status[3] == 0
    headers[0].min_y, headers[0].max_y = 5, 20
    assert execute([known])[0].chunk_status[3] == 0
    outside = make((1.5, 4.5, 1.5), (1, 0, 0), (0, 0, 0), tmax=2, lower=(0,)*3, upper=(1,)*3)
    assert execute([outside])[0].chunk_status[3] == 2
    leaving = make((1.5, 19.5, 1.5), (0, 1, 0), (0, 0, 0), tmax=2, lower=(0,)*3, upper=(1,)*3)
    hit = execute([leaving])[0]
    assert hit.chunk_status[3] == 2 and hit.distance_normal[0] == .5
    headers[0].macro_mask = 1;headers[0].leaf_offset = 0xffffffff
    assert execute([known])[0].chunk_status[3] == 2
    headers[0].leaf_offset = 3;headers[0].flags = 0
    assert execute([known])[0].chunk_status[3] == 2
    report = dict(status="passed", sparse_leaf_cases=len(rows), header_bytes=48, offsets="elements",
                  material_packing="even low16 / odd high16", partial_height_unknown=True,
                  invalid_payload_unknown=True, chunk_lookup="hash linear probing entry=index+1",
                  negative_coordinate_miss=True, generation_uniform=True,
                  production_slang_cpu=True, gpu_runtime=False)
    (ROOT / "logs/tools/voxel-buffer-contract.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report))


if __name__ == "__main__":
    run()
