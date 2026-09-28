"""Real listener/module/Box3D item scale; contact and graphics qualification remain separate."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile
import time

from make_tile_fixture import payload
from qualification_runtime import stage_runtime

ROOT = Path(__file__).resolve().parents[2]


def stats(values):
    data = sorted(values)
    if not data:
        raise RuntimeError("Missing measured samples")
    return {"samples": len(data), "median": data[len(data) // 2],
            "p95": data[math.ceil(len(data) * .95) - 1],
            "p99": data[math.ceil(len(data) * .99) - 1], "worst": data[-1]}


def summarize(path):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) < 300:
        raise RuntimeError("Insufficient profile rows: " + str(path))
    if len({row["schema"] for row in rows}) != 1 or any(int(row["schema"]) not in (1, 2) for row in rows) or any(
            int(b["pump"]) != int(a["pump"]) + 1 for a, b in zip(rows, rows[1:])):
        raise RuntimeError("Profile schema mismatch or dropped pump rows")
    # Profile tick is module/world clock, not accepted input sequence.
    initial_tick = int(rows[0]["authority_tick"])
    rows = [row for row in rows if int(row["authority_tick"]) >= initial_tick + 300]
    active = []
    previous = None
    for row in rows:
        tick = int(row["authority_tick"])
        if previous is not None and tick > previous:
            active.append(row)
        previous = tick
        phases = sum(float(row[key]) for key in ("control_wall_ms", "network_wall_ms",
                     "entity_wall_ms", "authority_session_wall_ms"))
        if abs(phases - float(row["total_wall_ms"])) > .00001:
            raise RuntimeError("Phase accounting mismatch")
    return {"warmup_authority_ticks": 300,
            "all_pumps_ms": stats([float(row["total_wall_ms"]) for row in rows]),
            "advancing_authority_pumps_ms": stats([float(row["total_wall_ms"]) for row in active]),
            "advancing_pump_process_alloc_bytes": stats([int(row["process_alloc_bytes"]) for row in active])}


def summarize_modules(path):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    rows = rows[300:]
    return {key: stats([float(row[key]) for row in rows]) for key in
            ("whole_tick_ms", "authority_schedule_ms", "module_schedule_ms")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, choices=(100, 1000, 10000), required=True)
    parser.add_argument("--awake", type=int, required=True)
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--timing-isolated", action="store_true",
                        help="Operator confirms no concurrent builds, graphics runs, or other heavy probes")
    args = parser.parse_args()
    if not 0 < args.awake <= args.count or not 15 <= args.seconds <= 110:
        parser.error("Require 1..count awake and 15..110 seconds")
    evidence = Path(tempfile.mkdtemp(prefix="item-listener-", dir=ROOT / "logs/server"))
    build = ROOT / "build/release-windows"
    client_bundle, server_bundle = stage_runtime(build, evidence)
    probe = args.probe or build / "tools/SessionTransportProbe/managed/SessionTransportProbe.dll"
    probe = probe.resolve(strict=True)
    probe_identity = dict(path=str(probe), sha256=hashlib.sha256(probe.read_bytes()).hexdigest())
    (evidence / "probe.json").write_text(json.dumps(probe_identity, indent=2))
    world = evidence / "world"
    world.mkdir()
    (world / "main.glb").write_bytes(payload(0, [.5, .5, .5, 1]))
    (world / "map.json").write_text(json.dumps(dict(version=1, map="main.glb", spawn=[0, 3, -9], yaw=0., pitch=0.)))
    env = {k: v for k, v in os.environ.items() if not k.startswith("OCTARYN_")}
    endpoint = evidence / "endpoint"
    env.update({"OCTARYN_SERVER_MAP_MODE": "1", "OCTARYN_SERVER_MAP_PATH": str(world / "main.glb"),
                "OCTARYN_SERVER_MAP_MANIFEST_PATH": str(world / "map.json"),
                "OCTARYN_SERVER_WORLD_DIR": str(world), "OCTARYN_SERVER_PLAYER_SAVE_ROOT": str(world),
                "OCTARYN_SERVER_LOCAL_ENDPOINT_PATH": str(endpoint),
                "OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY": "1",
                "OCTARYN_SERVER_LOOP_PROFILE": str(evidence / "loop.csv"),
                "OCTARYN_SERVER_MODULE_PROFILE": str(evidence / "module.csv"),
                "OCTARYN_SERVER_TICK_PROFILE": str(evidence / "authority.csv")})
    with (evidence / "server.log").open("w") as output:
        server = subprocess.Popen(["dotnet", str(probe), "--item-scale-server", str(server_bundle),
            str(args.count), str(args.awake), str(args.seconds + 5)], cwd=ROOT, env=env,
            stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 60
            while not endpoint.exists():
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Listener startup failed: " + str(evidence))
                time.sleep(.05)
            with (evidence / "client.log").open("w") as client_log:
                subprocess.run(["dotnet", str(probe), "--item-scale-client", str(client_bundle),
                    endpoint.read_text(), str(evidence / "client"), str(args.count), str(args.seconds)],
                    cwd=ROOT, stdout=client_log, stderr=subprocess.STDOUT, timeout=args.seconds + 15, check=True)
            if server.wait(timeout=15) != 0:
                raise RuntimeError("Authority failed: " + str(evidence))
        finally:
            if server.poll() is None:
                server.kill()
                server.wait(timeout=10)
    result = {"schema": 1, "status": "passed", "count": args.count, "awake": args.awake,
              "probe": probe_identity,
              "timing_qualified": args.timing_isolated,
              "timing_isolation": "operator confirmed" if args.timing_isolated else "not established",
              "qualification": "whole listener/module and typed item stream; no contact stress, graphics, or Bistro",
              "allocation_scope": "approximate process-wide GC allocation counter; includes workers",
              "module_ticks": summarize_modules(evidence / "module.csv"),
              **summarize(evidence / "loop.csv")}
    if "server_diagnostics_dropped" in (evidence / "server.log").read_text():
        raise RuntimeError("Diagnostic/profile queue dropped records; result is incomplete")
    (evidence / "result.json").write_text(json.dumps(result, indent=2))
    print("authority_item_listener=passed evidence=" + str(evidence))


if __name__ == "__main__":
    main()
