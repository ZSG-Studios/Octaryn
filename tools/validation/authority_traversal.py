"""Real-time tiled authority/transport traversal; no renderer or client prediction."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import tempfile
import time

from case_evidence import record_build
from make_tile_fixture import payload
from process_memory import process_memory
from qualification_runtime import stage_runtime

ROOT = Path(__file__).resolve().parents[2]


def memory_tail(samples, seconds):
    tail = [row for row in samples if row["seconds"] >= min(300, seconds/3)]
    if len(tail) < 5:
        raise RuntimeError("Insufficient process memory samples")
    result = {}
    for key in ("process_rss_bytes", "process_private_bytes"):
        values = [row[key] for row in tail]
        times = [row["seconds"] for row in tail]
        mx, my = statistics.mean(times), statistics.mean(values)
        slope = sum((x-mx)*(y-my) for x,y in zip(times, values)) / sum((x-mx)**2 for x in times)
        result[key] = dict(minimum=min(values), maximum=max(values), range=max(values)-min(values),
                           bytes_per_second=slope, passed=max(values)-min(values) <= 64*1024**2 and slope <= 4096)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=1800)
    args = parser.parse_args()
    if not 10 <= args.seconds <= 7200:
        parser.error("duration must be10..7200 seconds")
    evidence = Path(tempfile.mkdtemp(prefix="authority-traversal-", dir=ROOT / "logs/server"))
    world = evidence / "world"
    world.mkdir()
    for i in range(33):
        (world / f"tile-{i}.glb").write_bytes(payload(i*24, [.5,.5,.5,1]))
    (world / "map.json").write_text(json.dumps(dict(version=1, map="tile-32.glb", spawn=[0,3,-3], yaw=0., pitch=0.,
        tiles=[[i*24-12,-1,-12,i*24+12,6,12] for i in range(33)], tile_files=[f"tile-{i}.glb" for i in range(33)])))
    build = ROOT / "build/release-windows"
    bundle, server_bundle = stage_runtime(build, evidence)
    record_build(bundle, evidence)
    files = list(world.glob("*.glb")) + [world / "map.json"] + list(server_bundle.glob("*.dll")) + list(server_bundle.glob("*.exe"))
    (evidence / "identity.json").write_text(json.dumps({"schema_version":1, "duration_seconds":args.seconds,
        "dimensions":None, "rendering":False, "command_hz":60, "route_x":[8,760], "route_z":-3,
        "sha256":{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in files}}, indent=2))
    endpoint, shutdown = evidence / "server.endpoint", evidence / "shutdown.request"
    env = {k:v for k,v in os.environ.items() if not k.startswith("OCTARYN_")}
    env.update({"OCTARYN_SERVER_LISTEN":"127.0.0.1:0", "OCTARYN_SERVER_LOCAL_ENDPOINT_PATH":str(endpoint),
        "OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH":str(shutdown), "OCTARYN_SERVER_WORLD_DIR":str(world),
        "OCTARYN_SERVER_MAP_MODE":"1", "OCTARYN_SERVER_MAP_PATH":str(world / "tile-32.glb"),
        "OCTARYN_SERVER_MAP_MANIFEST_PATH":str(world / "map.json"), "OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY":"1"})
    client = None
    samples = []
    with (evidence / "server.log").open("wb") as server_log, (evidence / "client.log").open("wb") as client_log:
        server = subprocess.Popen([str(server_bundle / "Octaryn.Server.exe")], cwd=evidence, env=env,
                                  stdout=server_log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 60
            while not endpoint.exists():
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Authority startup/readiness failed: " + str(evidence))
                time.sleep(.1)
            command = ["dotnet", str(build / "tools/SessionTransportProbe/managed/SessionTransportProbe.dll"),
                "--traversal", str(bundle), endpoint.read_text().strip(), str(evidence / "client"), str(args.seconds)]
            (evidence / "command.json").write_text(json.dumps(command))
            client = subprocess.Popen(command, cwd=ROOT, env=env, stdout=client_log, stderr=subprocess.STDOUT)
            started = time.monotonic()
            with (evidence / "memory.jsonl").open("w") as memory_log:
                while client.poll() is None:
                    elapsed = time.monotonic() - started
                    if elapsed > args.seconds+90 or server.poll() is not None:
                        raise RuntimeError("Traversal deadline/authority exit: " + str(evidence))
                    sample = dict(seconds=elapsed, **process_memory(server.pid))
                    samples.append(sample)
                    memory_log.write(json.dumps(sample) + "\n")
                    memory_log.flush()
                    time.sleep(1)
            if client.returncode:
                raise RuntimeError("Traversal client failed: " + str(evidence))
        finally:
            if client and client.poll() is None:
                client.kill()
                client.wait(timeout=10)
            shutdown.write_text("stop\n")
            try:
                server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=10)
    if server.returncode:
        raise RuntimeError("Authority shutdown failed: " + str(evidence))
    log = (evidence / "server.log").read_text(errors="replace")
    records = [dict((key,int(value)) for key,value in re.findall(r"(\w+)=(\d+)", line))
               for line in log.splitlines() if line.startswith("server_collision_residency version=")]
    if not records or records[-1]["failed"] or records[-1]["evictions"] == 0 or "reason=no_floor" in log:
        raise RuntimeError("Missing/failed authority tile eviction evidence: " + str(evidence))
    if list((world / "remote-session").glob("*.json")):
        raise RuntimeError("Authority wrote production JSON mailboxes")
    if "item_settle id=2 count=1" not in log:
        raise RuntimeError("Dropped item did not settle on the fixture floor: " + str(evidence))
    tail = memory_tail(samples, args.seconds)
    if args.seconds >= 1800 and not all(row["passed"] for row in tail.values()):
        raise RuntimeError("Authority process memory failed plateau limits: " + str(evidence))
    result = json.loads((evidence / "client/result.json").read_text())
    result.update(status="passed" if args.seconds >= 1800 else "smoke_passed", memory_tail=tail, collision=records[-1],
                  memory_qualification=args.seconds >= 1800,
                  runtime_snapshot=str(evidence / "runtime-snapshot.json"), item_settled=True,
                  scope="authority/server transport traversal only; no renderer or client prediction qualification")
    (evidence / "result.json").write_text(json.dumps(result, indent=2))
    print("authority_traversal=" + result["status"] + " evidence=" + str(evidence))


if __name__ == "__main__":
    main()
