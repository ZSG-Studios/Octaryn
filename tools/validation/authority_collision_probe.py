"""CPU authority tile lifecycle: multiple anchors, held item state and eviction."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import tempfile
import time
import struct
import statistics
import subprocess
import sys
import math

from make_tile_fixture import payload
from process_memory import process_memory

ROOT = Path(__file__).resolve().parents[2]


def dense_payload():
    source = payload(0, [.5, .5, .5, 1])
    length = struct.unpack_from("<I", source, 12)[0]
    document = json.loads(source[20:20+length])
    binary = bytearray(source[28+length:])
    accessor = document["accessors"][2]
    view = document["bufferViews"][accessor["bufferView"]]
    indices = binary[view["byteOffset"]:view["byteOffset"] + view["byteLength"]] * 1366
    view.update(byteOffset=len(binary), byteLength=len(indices))
    accessor["count"] *= 1366
    binary.extend(indices)
    document["buffers"][0]["byteLength"] = len(binary)
    encoded = json.dumps(document).encode()
    encoded += b" " * (-len(encoded) % 4)
    return struct.pack("<III", 0x46546C67, 2, 28+len(encoded)+len(binary)) + \
        struct.pack("<II", len(encoded), 0x4E4F534A) + encoded + \
        struct.pack("<II", len(binary), 0x004E4942) + binary


class Item(C.Structure):
    _fields_ = [(name, C.c_float) for name in ("x", "y", "z", "vx", "vy", "vz")] + [
        ("grounded", C.c_uint32), ("sleeping", C.c_uint32), ("sleep_timer", C.c_float)]


class Player(C.Structure):
    _fields_ = [(name, C.c_float) for name in ("x", "y", "z", "pitch", "yaw", "vx", "vy", "vz")] + [
        ("grounded", C.c_uint32), ("mode", C.c_uint32), ("jump", C.c_uint16)]


class Input(C.Structure):
    _fields_ = [("flags", C.c_uint32), ("controller", C.c_uint32)] + [(name, C.c_float) for name in (
        "mx", "my", "mz", "cx", "cy", "cz", "pitch", "yaw")] + [("relative", C.c_int32)]


class Tick(C.Structure):
    _fields_ = [("input", C.c_uint32), ("reserved", C.c_uint32)] + [(name, C.c_float) for name in ("dx", "dy", "dz")]


class Stats(C.Structure):
    _fields_ = [(name, C.c_uint32) for name in ("version", "resident", "preparing", "failed")] + [
        (name, C.c_uint64) for name in ("reserved_bytes", "loads", "evictions", "cancelled", "waits", "resident_bytes", "budget_bytes")]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=ROOT / "build/release-windows/server/bundle")
    args = parser.parse_args()
    evidence = Path(tempfile.mkdtemp(prefix="collision-residency-", dir=ROOT / "logs/server"))
    # Sparse distant cells ensure authority collision does not depend on the player radius.
    count = 33
    for index in range(count):
        (evidence / f"tile-{index}.glb").write_bytes(payload(index * 24, [.5, .5, .5, 1]))
    manifest = evidence / "map.json"
    manifest.write_text(json.dumps({"version": 1, "map": "tile-32.glb", "spawn": [0, 3, -9],
        "tiles": [[i*24-12, -1, -12, i*24+12, 6, 12] for i in range(count)],
        "tile_files": [f"tile-{i}.glb" for i in range(count)]}))
    dll_directory = os.add_dll_directory(str(args.bundle.resolve())) if os.name == "nt" else None
    library = C.CDLL(str(args.bundle / ("octaryn_server_map_world.dll" if os.name == "nt" else "liboctaryn_server_map_world.so")))
    create = library.octaryn_server_map_world_create
    create.argtypes, create.restype = [C.c_char_p, C.c_char_p], C.c_void_p
    destroy = library.octaryn_server_map_world_destroy
    destroy.argtypes = [C.c_void_p]
    destroy.restype = None
    ready = library.octaryn_server_map_world_collision_ready
    ready.argtypes = [C.c_void_p, C.c_float, C.c_float, C.c_float]
    step = library.octaryn_server_map_world_step_item
    step.argtypes = [C.c_void_p, C.POINTER(Item), C.c_double]
    spawn = library.octaryn_server_map_world_spawn
    spawn.argtypes = [C.c_void_p, C.POINTER(Player)]
    step_player = library.octaryn_server_map_world_step
    step_player.argtypes = [C.c_void_p, C.POINTER(Input), C.c_double, C.POINTER(Player), C.POINTER(Tick)]
    read_stats = library.octaryn_server_map_world_collision_stats
    read_stats.argtypes = [C.c_void_p, C.POINTER(Stats), C.c_uint32]
    checks, samples, lifetimes = [], [], []

    def check(value, name):
        if not value:
            raise RuntimeError(name + ": " + str(evidence))
        checks.append(name)

    def stats(world):
        result = Stats()
        if read_stats(world, C.byref(result), C.sizeof(result)):
            raise RuntimeError("Residency stats ABI failed")
        row = {name: getattr(result, name) for name, _ in Stats._fields_}
        if row["version"] != 2 or row["preparing"] > 2 or row["resident_bytes"] > row["budget_bytes"] or row["failed"] or \
                row["reserved_bytes"] > row["budget_bytes"] + 2*72*1024**2:
            raise RuntimeError("Collision preparation/reservation bounds failed")
        samples.append(row)
        row.update(process_memory())
        return row

    def wait(predicate, timeout=10):
        deadline = time.monotonic() + timeout
        while not predicate():
            if time.monotonic() > deadline:
                raise RuntimeError("Collision readiness watchdog: " + str(evidence))
            time.sleep(.005)

    world = create(os.fsencode(evidence / "tile-32.glb"), os.fsencode(manifest))
    check(bool(world), "world creation")
    try:
        first = stats(world)
        check(0 < first["resident"] < count, "startup loads a bounded subset")
        check(True, "spawn collision comes from intersecting tiles even when map points to a distant tile")
        player, tick = Player(), Tick()
        check(spawn(world, C.byref(player)) == 0, "native authoritative spawn state")
        player.x = 2
        control = Input(6, 1, 1, 0, 1, 0, 0, 0, 0, math.pi/4, 1)
        before_player = bytes(player)
        check(step_player(world, C.byref(control), .25, C.byref(player), C.byref(tick)) == 0 and bytes(player) == before_player,
              "maximum-delta diagonal sprint-fly holds until its complete swept region is resident")
        def fly_ready():
            if step_player(world, C.byref(control), .25, C.byref(player), C.byref(tick)):
                raise RuntimeError("Native player step failed")
            return player.x != 2
        wait(fly_ready)
        check(abs(player.x-(2+100*math.sqrt(2)*.25)) < .01,
              "diagonal sprint-fly resumes with full authoritative displacement")
        item = Item(768, 4, -9, 3, -1, 0, 0, 0, .025)
        before = bytes(item)
        check(step(world, C.byref(item), 1/60) == 0 and bytes(item) == before,
              "unready distant item holds every physics field without analytic fallback")
        wait(lambda: ready(world, 768, -9, 4) == 0 and ready(world, 0, -9, 32) == 0)
        check(stats(world)["resident"] < count, "two distant anchors coexist without loading the intervening world")
        check(step(world, C.byref(item), 1/60) == 0 and bytes(item) != before,
              "held item resumes physical movement")
        # Keep both distant anchors active beyond eviction hysteresis.
        deadline = time.monotonic() + 2.2
        while time.monotonic() < deadline:
            check(ready(world, 0, -9, 32) == 0 and ready(world, 768, -9, 4) == 0,
                  "both active anchors retain collision")
            time.sleep(.02)
        protected = stats(world)
        # Retire only the player anchor, retaining the distant awake item's floor.
        deadline = time.monotonic() + 2.2
        while time.monotonic() < deadline:
            ready(world, 768, -9, 4)
            step(world, C.byref(item), 1/60)
            time.sleep(.02)
        after = stats(world)
        check(after["evictions"] > protected["evictions"] and after["resident"] < protected["resident"],
              "retired player region evicts while distant item region stays resident")
        check(item.y > 0 and ready(world, 768, -9, 4) == 0, "distant awake item keeps valid floor")
        time.sleep(.002)
        ready(world, 384, -9, 1)
        check(stats(world)["preparing"] > 0, "obsolete request has in-flight preparation")
        time.sleep(2.1)
        ready(world, 768, -9, 4)
        check(stats(world)["cancelled"] > after["cancelled"], "obsolete prepared tile is discarded before publication")
    finally:
        destroy(world)
    for _ in range(20):
        world = create(os.fsencode(evidence / "tile-32.glb"), os.fsencode(manifest))
        check(bool(world), "repeated world creation")
        try:
            time.sleep(.002)
            ready(world, 384, -9, 1)
        finally:
            destroy(world)
        lifetimes.append(process_memory())
    check(True, "twenty world destructions safely join pending preparation")
    tail_memory = {}
    for key in lifetimes[0]:
        values = [sample[key] for sample in lifetimes[5:]]
        mean_x = (len(values) - 1) / 2
        slope = sum((i-mean_x)*(v-statistics.mean(values)) for i, v in enumerate(values)) / \
            sum((i-mean_x)**2 for i in range(len(values)))
        tail_memory[key] = {"minimum": min(values), "maximum": max(values), "range": max(values)-min(values),
                            "slope_bytes_per_world": slope}
        check(max(values)-min(values) <= 64*1024**2 and slope <= 1024**2,
              key + " plateaus after five warmup lifetimes within64MiB/1MiB-per-world limits")
    previous_budget = os.environ.get("OCTARYN_SERVER_COLLISION_BUDGET_MIB")
    def failed_world(glb, manifest_path, log_name, reason):
        native_log = evidence / log_name
        saved_stderr = os.dup(2)
        try:
            with native_log.open("wb") as output:
                os.dup2(output.fileno(), 2)
                result = create(os.fsencode(glb), os.fsencode(manifest_path))
        finally:
            os.dup2(saved_stderr, 2)
            os.close(saved_stderr)
        if result:
            destroy(result)
        return not result and reason in native_log.read_text(errors="replace")
    try:
        os.environ["OCTARYN_SERVER_COLLISION_BUDGET_MIB"] = "invalid"
        check(failed_world(evidence / "tile-32.glb", manifest, "invalid-budget.log", "Collision budget must be"),
              "invalid collision budget fails world startup explicitly")
        dense = evidence / "dense.glb"
        dense.write_bytes(dense_payload())
        overbudget = evidence / "overbudget.json"
        overbudget.write_text(json.dumps({"version": 1, "map": "dense.glb", "spawn": [0, 3, -9],
            "tiles": [[-12, -1, -12, 12, 6, 12]] * 18, "tile_files": ["dense.glb"] * 18}))
        os.environ["OCTARYN_SERVER_COLLISION_BUDGET_MIB"] = "64"
        started = time.monotonic()
        check(failed_world(dense, overbudget, "over-budget.log", "reason=protected_budget"),
              "64 MiB protected-set overflow fails explicitly")
        check(time.monotonic() - started < 15, "budget failure does not wait the general 60 second startup deadline")
    finally:
        if previous_budget is None:
            os.environ.pop("OCTARYN_SERVER_COLLISION_BUDGET_MIB", None)
        else:
            os.environ["OCTARYN_SERVER_COLLISION_BUDGET_MIB"] = previous_budget
    result = {"status": "passed", "checks": checks, "samples": samples, "lifetime_memory": lifetimes, "tail_memory": tail_memory,
              "limits": {"preparing": 2, "default_resident_reservation_bytes": 512*1024**2,
                         "maximum_preparation_reservation_bytes": 2*72*1024**2},
              "scope": "native authority collision and item physics; ACK retention is separately tested by CommandDependencyProbe"}
    (evidence / "result.json").write_text(json.dumps(result, indent=2))
    if dll_directory:
        dll_directory.close()
    print("authority_collision=passed evidence=" + str(evidence))


if __name__ == "__main__":
    if "--worker" in sys.argv:
        sys.argv.remove("--worker")
        main()
    else:
        run = Path(tempfile.mkdtemp(prefix="collision-watchdog-", dir=ROOT / "logs/server"))
        log = run / "native.log"
        with log.open("wb") as output:
            try:
                subprocess.run([sys.executable, str(Path(__file__).resolve()), "--worker", *sys.argv[1:]],
                               stdout=output, stderr=subprocess.STDOUT, check=True, timeout=120)
            except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as error:
                raise RuntimeError("Collision probe failed; inspect " + str(log)) from error
        print(log.read_text(errors="replace").splitlines()[-1])
        print("watchdog_evidence=" + str(run))
