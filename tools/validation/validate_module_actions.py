"""Verify UI action bursts and authority receipts over local and remote sessions."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time

from capture_ui_effects import isolated_environment, stop_timed_out_client, ROOT
from capture_regions import save_png
from validate_rhi_client_diagnostic import inspect_result


def run_case(bundle, server_bundle, evidence, remote, port):
    case = evidence / ("remote" if remote else "local")
    case.mkdir()
    env = isolated_environment(case, "dx12", 2560, 1440)
    env.update(OCTARYN_CLIENT_CAPTURE_PATH=str(case / "frame.bmp"),
               OCTARYN_CLIENT_CAPTURE_MIN_FRAME="180")
    command = [str(bundle / "Octaryn.Client.exe"), "--validate-module-actions",
               "--frames", "320", "--validate-ui", "--benchmark-hidden", "--benchmark-settings"]
    server = None
    shutdown = case / "server.shutdown"
    print(f"module_actions_case=running remote={remote} evidence={case}", flush=True)
    with (case / "server.log").open("wb") as server_log:
        try:
            if remote:
                server_env = env.copy()
                maps = bundle / "Client/Assets/Maps"
                server_env.update(OCTARYN_SERVER_MAP_MODE="1",
                                  OCTARYN_SERVER_MAP_PATH=str(maps / "main.glb"),
                                  OCTARYN_SERVER_MAP_MANIFEST_PATH=str(maps / "map.json"),
                                  OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH=str(shutdown))
                server = subprocess.Popen([str(server_bundle / "Octaryn.Server.exe"),
                                           "--listen", f"127.0.0.1:{port}",
                                           "--world-dir", str(case / "server-world")],
                                          cwd=case, env=server_env, stdout=server_log,
                                          stderr=subprocess.STDOUT)
                deadline = time.monotonic() + 60
                while "server_remote_listening" not in (case / "server.log").read_text(errors="replace"):
                    if server.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError(f"Dedicated server failed to start: {case}")
                    time.sleep(.1)
                command += ["--connect", f"127.0.0.1:{port}"]
            (case / "command.json").write_text(json.dumps(command, indent=2))
            with (case / "client.log").open("wb") as output:
                client = subprocess.Popen(command, cwd=case, env=env, stdout=output,
                                          stderr=subprocess.STDOUT)
                try:
                    code = client.wait(timeout=240)
                except subprocess.TimeoutExpired:
                    stop_timed_out_client(client, case)
                    raise RuntimeError(f"Client timed out: {case}")
            text = (case / "client.log").read_text(errors="replace")
            frames, primitives, _ = inspect_result(code, text, "D3D12", minimum_frames=320)
            for marker in ("module_action_validation=passed drops=2 unique_receipts=2",
                           "rml_ui_contract=passed", "item_target_contract=passed"):
                if marker not in text:
                    raise RuntimeError(f"Missing {marker}: {case}")
            receipts = re.findall(r"module_event id=(\d+) kind=2 item=(\d+) count=(\d+)", text)
            if len(receipts) != 2 or len({event[0] for event in receipts}) != 2:
                raise RuntimeError(f"Missing or duplicate drop receipts: {case}")
            if not (case / "frame.bmp").is_file():
                raise RuntimeError(f"No actual world GPU capture: {case}")
            save_png(case / "frame.bmp", case / "frame.png")
            result = dict(status="passed", remote=remote, exit_code=code, frames=frames,
                          map_primitives=primitives, receipts=receipts)
        finally:
            if server:
                shutdown.write_text("stop\n")
                try:
                    server.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()
                if server.returncode != 0:
                    raise RuntimeError(f"Dedicated server exit={server.returncode}: {case}")
    (case / "result.json").write_text(json.dumps(result, indent=2))
    print(f"module_actions_case=passed remote={remote} evidence={case}", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-bundle", type=Path, default=ROOT / "build/release-windows/client/bundle")
    parser.add_argument("--server-bundle", type=Path, default=ROOT / "build/release-windows/server/bundle")
    parser.add_argument("--evidence-root", type=Path, default=ROOT / "logs/client/module-actions")
    parser.add_argument("--port", type=int, default=17562)
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="run-", dir=args.evidence_root.resolve()))
    results = [run_case(args.client_bundle.resolve(), args.server_bundle.resolve(), evidence,
                        remote, args.port) for remote in (False, True)]
    (evidence / "results.json").write_text(json.dumps(results, indent=2))
    print(f"module_actions_validation=passed evidence={evidence}")


if __name__ == "__main__":
    main()
