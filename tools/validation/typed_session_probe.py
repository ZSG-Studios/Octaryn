"""Real headless authority + client transport with bounded network impairment."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

from network.udp_impairment import UdpImpairment
from qualification_runtime import stage_runtime

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rtt-ms", type=int, default=0)
    parser.add_argument("--jitter-ms", type=int, default=0)
    parser.add_argument("--loss-percent", type=float, default=0)
    parser.add_argument("--reconnect", action="store_true")
    parser.add_argument("--backpressure", action="store_true")
    parser.add_argument("--probe", type=Path)
    args = parser.parse_args()
    evidence = Path(tempfile.mkdtemp(prefix="typed-session-", dir=ROOT / "logs/server"))
    build = ROOT / "build/release-windows"
    bundle, server_bundle = stage_runtime(build, evidence)
    maps = build / "client/bundle/Client/Assets/Maps"
    env = {k: v for k, v in os.environ.items() if not k.startswith("OCTARYN_")}
    endpoint = evidence / "server.endpoint"
    stop = evidence / "shutdown.request"
    env.update({
        "OCTARYN_SERVER_LISTEN": "127.0.0.1:0",
        "OCTARYN_SERVER_LOCAL_ENDPOINT_PATH": str(endpoint),
        "OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH": str(stop),
        "OCTARYN_SERVER_WORLD_DIR": str(evidence / "world"),
        "OCTARYN_SERVER_MAP_MODE": "1",
        "OCTARYN_SERVER_MAP_PATH": str(maps / "main.glb"),
        "OCTARYN_SERVER_MAP_MANIFEST_PATH": str(maps / "map.json"),
        "OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY": "1",
        "OCTARYN_SERVER_TICK_PROFILE": str(evidence / "authority.csv"),
        "OCTARYN_SERVER_LOOP_PROFILE": str(evidence / "loop.csv"),
        "OCTARYN_SERVER_MODULE_PROFILE": str(evidence / "module.csv"),
    })
    with (evidence / "server.log").open("w") as log:
        server = subprocess.Popen([str(server_bundle / "Octaryn.Server.exe")],
                                  cwd=evidence, env=env, stdout=log, stderr=subprocess.STDOUT)
        proxy = None
        try:
            deadline = time.monotonic() + 60
            while not endpoint.exists():
                if server.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("Authority failed readiness: " + str(evidence))
                time.sleep(0.1)
            port = int(endpoint.read_text().split(":")[-1])
            half = args.rtt_ms / 2
            proxy = UdpImpairment(port, minimum_ms=max(0, half - args.jitter_ms),
                                  maximum_ms=half + args.jitter_ms, loss_percent=args.loss_percent)
            proxy.start()
            command = ["dotnet", str(args.probe or build / "tools/SessionTransportProbe/managed/SessionTransportProbe.dll"),
                       "--backpressure" if args.backpressure else "--reconnect" if args.reconnect else "--live", str(bundle), f"127.0.0.1:{proxy.address[1]}", str(evidence / "client")]
            with (evidence / "client.log").open("w") as client_log:
                subprocess.run(command, cwd=ROOT, stdout=client_log, stderr=subprocess.STDOUT,
                               check=True, timeout=35 if args.backpressure else 25)
            if args.reconnect:
                proxy.client = None
                command[2] = "--live"
                command[-1] = str(evidence / "new-client")
                with (evidence / "new-client.log").open("w") as client_log:
                    subprocess.run(command, cwd=ROOT, stdout=client_log, stderr=subprocess.STDOUT,
                                   check=True, timeout=25)
            if list((evidence / "world/remote-session").glob("*.json")):
                raise RuntimeError("Authority wrote production JSON mailboxes")
        finally:
            if proxy:
                (evidence / "network.json").write_text(json.dumps(proxy.stop(), indent=2))
            stop.write_text("stop\n")
            try:
                server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=10)
    if server.returncode != 0:
        raise RuntimeError(f"Authority exit={server.returncode}")
    print("typed_session_probe=passed evidence=" + str(evidence))


if __name__ == "__main__":
    main()
