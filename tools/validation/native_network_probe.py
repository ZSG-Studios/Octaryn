"""Measure native Box3D prediction/replay over impaired real server transport."""
import argparse
import csv
import json
import math
import hashlib
from pathlib import Path
import statistics
import subprocess
import tempfile
import time

from capture_ui_effects import isolated_environment
from make_tile_fixture import payload
from network.udp_impairment import UdpImpairment
from case_evidence import record_build

ROOT = Path(__file__).resolve().parents[2]


def distribution(values):
    values = sorted(values)
    return dict(median=statistics.median(values), p95=values[int((len(values)-1)*.95)],
                p99=values[int((len(values)-1)*.99)], worst=values[-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rtt-ms", type=int, default=0)
    parser.add_argument("--jitter-ms", type=int, default=0)
    parser.add_argument("--loss-percent", type=float, default=0)
    parser.add_argument("--backend", choices=("dx12", "vulkan"), default="dx12")
    args = parser.parse_args()
    evidence = Path(tempfile.mkdtemp(prefix="native-network-", dir=ROOT / "logs/client"))
    build = ROOT / "build/release-windows"
    environment = isolated_environment(evidence, args.backend, 2560, 1440)
    world = evidence / "world"
    (world / "main.glb").write_bytes(payload(0, [.5,.5,.5,1]))
    manifest = world / "map.json"
    manifest.write_text(json.dumps(dict(version=1, map="main.glb", spawn=[0,3,6], yaw=0., pitch=-.1)))
    record_build(build / "client/bundle", evidence)
    (evidence / "asset-build.json").write_text(json.dumps({path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (manifest, world / "main.glb")}, indent=2))
    endpoint, stop = evidence / "server.endpoint", evidence / "shutdown.request"
    environment.update({"OCTARYN_SERVER_LISTEN": "127.0.0.1:0",
        "OCTARYN_SERVER_LOCAL_ENDPOINT_PATH": str(endpoint), "OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH": str(stop),
        "OCTARYN_SERVER_WORLD_DIR": str(evidence / "authority"), "OCTARYN_SERVER_MAP_MODE": "1",
        "OCTARYN_SERVER_MAP_PATH": str(world / "main.glb"), "OCTARYN_SERVER_MAP_MANIFEST_PATH": str(manifest),
        "OCTARYN_CLIENT_PREDICTION_PROFILE": str(evidence / "prediction.csv"),
        "OCTARYN_CLIENT_SCRIPTED_MOVE": "1.5"})
    with (evidence / "server.log").open("wb") as output:
        server = subprocess.Popen([str(build / "server/bundle/Octaryn.Server.exe")],
                                  cwd=evidence, env=environment, stdout=output, stderr=subprocess.STDOUT)
        proxy, client = None, None
        try:
            deadline = time.monotonic() + 60
            while not endpoint.exists():
                if server.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("Server readiness failed: " + str(evidence))
                time.sleep(.1)
            proxy = UdpImpairment(int(endpoint.read_text().split(":")[-1]),
                minimum_ms=max(0, args.rtt_ms/2 - args.jitter_ms),
                maximum_ms=args.rtt_ms/2 + args.jitter_ms, loss_percent=args.loss_percent)
            proxy.start()
            command = [str(build / "client/bundle/Octaryn.Client.exe"), "--connect",
                f"127.0.0.1:{proxy.address[1]}", "--frames", "360", "--benchmark-hidden", "--benchmark-settings"]
            (evidence / "command.json").write_text(json.dumps(command, indent=2))
            with (evidence / "client.log").open("wb") as log:
                client = subprocess.Popen(command, cwd=evidence, env=environment, stdout=log, stderr=subprocess.STDOUT)
                code = client.wait(timeout=120)
                if code: raise RuntimeError(f"Native client failed exit={code}: {evidence}")
        finally:
            if client and client.poll() is None:
                client.terminate()
                client.wait(timeout=10)
            if proxy: (evidence / "network.json").write_text(json.dumps(proxy.stop(), indent=2))
            stop.write_text("stop\n")
            try: server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=10)
    if server.returncode != 0:
        raise RuntimeError(f"Authority failed exit={server.returncode}: {evidence}")
    with (evidence / "prediction.csv").open() as source: rows = list(csv.DictReader(source))
    if len(rows) < 120: raise RuntimeError("Insufficient native prediction samples: " + str(evidence))
    for row in rows:
        if any(not math.isfinite(float(row[key])) for key in
               ("input_ack_ms", "correction_m", "authority_x", "authority_y", "authority_z")):
            raise RuntimeError("Nonfinite native prediction telemetry: " + str(evidence))
    steady = rows[30:]
    latency = distribution([float(row["input_ack_ms"]) for row in steady if float(row["input_ack_ms"]) >= 0])
    correction = distribution([float(row["correction_m"]) for row in steady])
    moving = [row for previous,row in zip(rows,rows[1:])
              if abs(float(row["authority_z"])-float(previous["authority_z"])) > 1e-5]
    def phase(group):
        return dict(samples=len(group), input_ack_ms=distribution([float(row["input_ack_ms"]) for row in group]),
                    correction_m=distribution([float(row["correction_m"]) for row in group]))
    movement = max(float(row["authority_z"]) for row in rows) - min(float(row["authority_z"]) for row in rows)
    replays = sum(int(row["replayed"]) for row in rows)
    all_correction=distribution([float(row["correction_m"]) for row in rows])
    if movement < 2 or replays == 0 or not moving or all_correction["p99"] > 1 or all_correction["worst"] > 2.5:
        raise RuntimeError("Prediction movement/replay/correction acceptance failed: " + str(evidence))
    result = dict(status="passed", workload="native_box3d_small_map", impairment=vars(args),
                  input_ack_ms=latency, correction_m=correction, movement_m=movement, replays=replays,
                  startup=phase(rows[:30]), moving=phase(moving), all_samples=phase(rows),
                  maximum_pending=max(int(row["pending"]) for row in rows), samples=len(rows))
    (evidence / "result.json").write_text(json.dumps(result, indent=2))
    print("native_network_probe=passed evidence=" + str(evidence))


if __name__ == "__main__":
    main()
