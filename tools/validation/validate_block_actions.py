"""Capture provisional block edits and authoritative accept/reject reconciliation."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

from validate_rhi_client_diagnostic import inspect_result


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-bundle", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix="block-actions-", dir=args.evidence_root.resolve()))
    executable = args.client_bundle.resolve() / "Octaryn.Client.exe"
    settings = {"version": 8, "windowWidth": 1280, "windowHeight": 720,
                "fullscreen": False, "renderDistance": 4,
                "upscalerMode": 0, "presentModeIndex": 0}
    (case / "settings.json").write_text(json.dumps(settings))
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("OCTARYN_CLIENT_", "OCTARYN_SERVER_"))}
    environment.update(OCTARYN_CLIENT_WORLD_PATH=str(case / "world"),
                       OCTARYN_CLIENT_SETTINGS_PATH=str(case / "settings.json"),
                       OCTARYN_CLIENT_INVENTORY_PATH=str(case / "inventory.json"),
                       OCTARYN_CLIENT_PROFILE_PATH=str(case / "profile.csv"),
                       OCTARYN_CLIENT_CAPTURE_PATH=str(case / "blocks.bmp"),
                       OCTARYN_CLIENT_GRAPHICS_API="dx12",
                       OCTARYN_CLIENT_RHI_VALIDATION="1", OCTARYN_CLIENT_UPSCALER="off")
    command = [str(executable), "--validate-block-actions", "--benchmark-hidden"]
    report = {"status": "running", "command": command, "evidence": str(case)}
    print("block_actions_started evidence=" + str(case), flush=True)
    try:
        with (case / "client.log").open("w") as output:
            process = subprocess.Popen(command, cwd=case, env=environment,
                                       stdout=output, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=150)
            except subprocess.TimeoutExpired:
                runtime = case / "world/runtime"
                runtime.mkdir(parents=True, exist_ok=True)
                (runtime / "shutdown.request").write_text("stop\n")
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    process.wait(timeout=10)
                raise RuntimeError("Block action qualification timed out")
        text = (case / "client.log").read_text(errors="replace")
        frames, columns, quads = inspect_result(code, text, api="D3D12", minimum_frames=0)
        if "block_actions_validation=passed captures=7 os_events_injected=0" not in text:
            raise RuntimeError("Missing seven-capture block reconciliation proof")
        captures = [case / "blocks.bmp"] + [case / f"blocks.bmp.sample-{index}.bmp" for index in range(1, 7)]
        for capture in captures:
            header = capture.read_bytes()[:54]
            if len(header) != 54 or header[:2] != b"BM":
                raise RuntimeError("Invalid GPU capture: " + str(capture))
            width, height = struct.unpack_from("<ii", header, 18)
            if width <= 0 or height == 0:
                raise RuntimeError("Empty GPU capture: " + str(capture))
        server_log = case / "world/logs/server/local-session.log"
        if not server_log.is_file() or "octaryn_server_shutdown=1" not in server_log.read_text(errors="replace"):
            raise RuntimeError("Local authority did not report graceful shutdown")
        report.update(status="passed", frames=frames, columns=columns, quads=quads,
                      captures=[str(path) for path in captures])
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        (case / "result.json").write_text(json.dumps(report, indent=2))
    print("block_actions_gpu=passed evidence=" + str(case), flush=True)


if __name__ == "__main__":
    run()
