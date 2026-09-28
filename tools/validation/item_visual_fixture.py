"""Serve original item meshes over real authority transport for graphical capture."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time

from make_tile_fixture import payload
from qualification_runtime import stage_runtime

ROOT = Path(__file__).resolve().parents[2]


def validate_completion(log, count, awake):
    records = [json.loads(line.split("=", 1)[1]) for line in log.splitlines()
               if line.startswith("authority_item_listener=")]
    if len(records) != 1:
        raise ValueError("Missing or ambiguous authority completion evidence")
    proof = records[0]
    expected = dict(status="passed", count=count, awake=awake,
                    settledBeforeMeasurement=count, finalAwake=awake, quantityConserved=True)
    if any(proof.get(key) != value for key, value in expected.items()):
        raise ValueError("Authority item conservation or workload evidence failed")
    return proof


def pose_evidence(log, count, awake):
    records = [json.loads(line.split("=", 1)[1]) for line in log.splitlines()
               if line.startswith("authority_item_poses=")]
    if [record.get("phase") for record in records] != ["initial", "final"]:
        raise ValueError("Missing or ambiguous authority pose observations")
    initial, final = [record["poses"] for record in records]
    identity = lambda poses: [(p["EntityId"], p["Generation"], p["ItemId"], p["Count"]) for p in poses]
    if len(initial) != count or len(final) != count or identity(initial) != identity(final):
        raise ValueError("Authority item identity/quantity changed")
    if len({p["EntityId"] for p in initial}) != count or any(p["Count"] != 1 for p in initial):
        raise ValueError("Authority item count conservation failed")
    stable = initial == final
    if awake == 0 and not stable:
        raise ValueError("Sleeping authority poses changed")
    return dict(initial=initial, final=final, stable=stable)


def wait_for_owner_stop(server, stop_path, seconds):
    deadline = time.monotonic() + seconds + 15
    requested = False
    while server.poll() is None:
        if not requested and stop_path.exists():
            server.stdin.write("stop\n")
            server.stdin.flush()
            requested = True
            deadline = min(deadline, time.monotonic() + 15)
        if time.monotonic() >= deadline:
            raise TimeoutError("Item fixture did not finish within its shutdown deadline")
        time.sleep(.05)
    return server.returncode


def floor_payload():
    source = payload(0, [.35, .4, .45, 1])
    length = struct.unpack_from("<I", source, 12)[0]
    doc = json.loads(source[20:20 + length])
    # Keep only the first floor quad; unused buffer data is permitted by glTF.
    doc["accessors"][2]["count"] = 6
    for index in (0, 1, 3):
        doc["accessors"][index]["count"] = 4
    doc["accessors"][0]["min"] = [-12, 0, -12]
    doc["accessors"][0]["max"] = [12, 0, 12]
    encoded = json.dumps(doc, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    binary_chunk = source[20 + length:]
    return struct.pack("<III", 0x46546C67, 2, 20 + len(encoded) + len(binary_chunk)) + \
        struct.pack("<II", len(encoded), 0x4E4F534A) + encoded + binary_chunk


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, choices=(4, 1000), default=4)
    parser.add_argument("--awake", type=int, default=4)
    parser.add_argument("--seconds", type=int, default=180)
    parser.add_argument("--probe", type=Path)
    args = parser.parse_args()
    if not 0 <= args.awake <= args.count or not 15 <= args.seconds <= 3600:
        parser.error("Invalid awake count or duration")
    evidence = Path(tempfile.mkdtemp(prefix="item-visual-", dir=ROOT / "logs/server"))
    build = ROOT / "build/release-windows"
    _, server_bundle = stage_runtime(build, evidence)
    world = evidence / "world"
    world.mkdir()
    (world / "main.glb").write_bytes(floor_payload())
    manifest = world / "map.json"
    manifest.write_text(json.dumps(dict(version=1, map="main.glb", spawn=[0, 2, -11],
                                        yaw=3.14159265, pitch=-.28)))
    endpoint = evidence / "endpoint"
    shutdown_request = evidence / "stop-request"
    env = {k: v for k, v in os.environ.items() if not k.startswith("OCTARYN_")}
    env.update({"OCTARYN_SERVER_MAP_MODE": "1", "OCTARYN_SERVER_MAP_PATH": str(world / "main.glb"),
        "OCTARYN_SERVER_MAP_MANIFEST_PATH": str(manifest), "OCTARYN_SERVER_WORLD_DIR": str(world),
        "OCTARYN_SERVER_PLAYER_SAVE_ROOT": str(world), "OCTARYN_SERVER_LOCAL_ENDPOINT_PATH": str(endpoint),
        "OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY": "1"})
    probe = args.probe or build / "tools/SessionTransportProbe/managed/SessionTransportProbe.dll"
    probe = probe.resolve(strict=True)
    probe_identity = dict(path=str(probe), sha256=hashlib.sha256(probe.read_bytes()).hexdigest())
    (evidence / "probe.json").write_text(json.dumps(probe_identity, indent=2))
    command = ["dotnet", str(probe), "--item-scale-server", str(server_bundle), str(args.count),
               str(args.awake), str(args.seconds), "visible-toss"]
    with (evidence / "server.log").open("w") as output:
        server = subprocess.Popen(command, cwd=ROOT, env=env, stdout=output, stderr=subprocess.STDOUT,
                                  stdin=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 60
            while not endpoint.exists():
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Item fixture startup failed: " + str(evidence))
                time.sleep(.05)
            result = dict(schema=2, status="running", endpoint=endpoint.read_text(), manifest=str(manifest),
                          count=args.count, awake=args.awake, pid=server.pid, seconds=args.seconds,
                          probe=probe_identity,
                          shutdown_request_path=str(shutdown_request),
                          fixture=("authoritative sleeping items after 240 real settling ticks" if args.awake == 0 else
                                   "real contact relaunch; tool reflection cost excludes CPU budget qualification"),
                          pose_parity_qualified=False, cpu_budget_qualified=False,
                          camera=[0, 2, -11, 3.14159265, -.28])
            (evidence / "fixture.json").write_text(json.dumps(result, indent=2))
            print(json.dumps(result), flush=True)
            if wait_for_owner_stop(server, shutdown_request, args.seconds) != 0:
                raise RuntimeError("Item fixture failed: " + str(evidence))
            log = (evidence / "server.log").read_text()
            result["authority_completion"] = validate_completion(log, args.count, args.awake)
            result["authority_poses"] = pose_evidence(log, args.count, args.awake)
            result["status"] = "passed"
            (evidence / "fixture.json").write_text(json.dumps(result, indent=2))
        finally:
            if server.poll() is None:
                server.kill()
                server.wait(timeout=10)
            server.stdin.close()
    print("item_visual_fixture=passed evidence=" + str(evidence), flush=True)


if __name__ == "__main__":
    main()
