"""Run the packaged RHI client; keep an isolated world and log as evidence."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def native_backend():
    return "dx12" if os.name == "nt" else ("metal" if sys.platform == "darwin" else "vulkan")


def inspect_result(returncode, text, api=None, minimum_frames=180):
    api = api or {"dx12": "D3D12", "metal": "Metal", "vulkan": "Vulkan"}[native_backend()]
    match = re.search(
        r"open_world_exit code=(\d+) frames=(\d+) columns=(\d+) quads=(\d+) gpu_bytes=(\d+)", text
    )
    if returncode or not match:
        raise RuntimeError("packaged diagnostic failed or omitted its final world counters")
    code, frames, columns, quads, gpu_bytes = map(int, match.groups())
    if code or frames < minimum_frames or min(columns, quads, gpu_bytes) <= 0:
        raise RuntimeError(f"incomplete diagnostic counters: {match.group(0)}")
    start = re.search(r"open_world_start .* radius=(\d+) authority=local_server", text)
    if not start or columns != (2 * int(start.group(1)) + 1) ** 2:
        raise RuntimeError("diagnostic did not retain its complete configured terrain window")
    if "authoritative_player_ready" not in text:
        raise RuntimeError("diagnostic did not receive an authoritative player")
    if f"world_device backend=slang_rhi api={api}" not in text:
        raise RuntimeError(f"diagnostic did not create the requested standalone {api} RHI device")
    if any(marker in text for marker in (
        "Validation Error", "Validation Warning", "VUID-",
        "rhi_validation severity=error", "rhi_validation severity=warning",
    )):
        raise RuntimeError("GPU or shader validation reported a warning/error; inspect retained log")
    return frames, columns, quads


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-bundle-root", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    args = parser.parse_args()
    bundle = args.client_bundle_root.resolve()
    executable = bundle / ("Octaryn.Client.exe" if os.name == "nt" else "Octaryn.Client")
    if not executable.is_file():
        raise RuntimeError(f"packaged client missing: {executable}")
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="run-", dir=args.evidence_root.resolve()))
    world = evidence / "world"
    world.mkdir()
    environment = {key: value for key, value in os.environ.items()
                   if not key.upper().startswith("OCTARYN_")}
    settings = evidence / "settings.json"
    settings.write_text(json.dumps({"windowWidth": 1280, "windowHeight": 720,
                        "fullscreen": False, "renderDistance": 4,
                        "upscalerMode": 1, "fsrDynamicResolution": 0}), encoding="utf-8")
    environment.update({"OCTARYN_CLIENT_SETTINGS_PATH": str(settings),
                        "OCTARYN_CLIENT_LIGHTING_PATH": str(evidence / "lighting.json"),
                        "OCTARYN_CLIENT_INVENTORY_PATH": str(evidence / "inventory.json")})
    environment["OCTARYN_CLIENT_GRAPHICS_API"] = native_backend()
    environment["OCTARYN_CLIENT_WORLD_PATH"] = str(world)
    environment["OCTARYN_CLIENT_RHI_VALIDATION"] = "1"
    log = evidence / "client.log"
    with log.open("wb") as output:
        process = subprocess.Popen(
            [str(executable), "--diagnostic", "--render-distance", "4", "--validate-ui", "--benchmark-hidden"], cwd=evidence, env=environment,
            stdout=output, stderr=subprocess.STDOUT,
        )
        try:
            result = process.wait(timeout=120)
        except subprocess.TimeoutExpired:
            # Ask only this diagnostic's server to stop before terminating its client.
            runtime = world / "runtime"
            runtime.mkdir(exist_ok=True)
            (runtime / "shutdown.request").write_text("stop\n", encoding="utf-8")
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            raise RuntimeError(f"packaged diagnostic timed out; evidence: {evidence}")
    frames, columns, quads = inspect_result(result, log.read_text(encoding="utf-8", errors="replace"))
    print(f"rhi_client_diagnostic=passed frames={frames} columns={columns} quads={quads} evidence={evidence}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        print(f"rhi_client_diagnostic=failed: {error}", file=sys.stderr)
        sys.exit(1)
