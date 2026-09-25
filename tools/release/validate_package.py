"""Qualify a relocated native Windows/Linux package using isolated GPU execution."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import struct
import subprocess
import sys
import time


def sha256(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def shutdown(process, world):
    runtime = world / "runtime"
    runtime.mkdir(parents=True, exist_ok=True)
    (runtime / "shutdown.request").write_text("stop\n", encoding="utf-8")
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def inspect(text, result, capture, api, radius, report):
    failures = report["failures"]
    if result != 0:
        failures.append(f"Client exit code was {result}")
    markers = (
        "authoritative_player_ready", "world_validation core=required",
        f"world_device backend=slang_rhi api={'D3D12' if api == 'dx12' else 'Vulkan'}",
        "rml_ui_contract=passed", "rml_ui_inventory_contract=passed",
        "rml_ui renderer=slang-rhi frame=", "world_capture",
        "world_fsr2 version=2.2.1",
    )
    for marker in markers:
        if marker not in text:
            failures.append(f"Missing runtime marker: {marker}")
    start = re.search(r"open_world_start .* radius=(\d+) authority=local_server", text)
    if not start or int(start.group(1)) != radius:
        failures.append("Actual startup radius/local authority did not match request")
    final = re.search(
        r"open_world_exit code=(\d+) frames=(\d+) columns=(\d+) quads=(\d+) gpu_bytes=(\d+)", text
    )
    if not final:
        failures.append("Missing final world counters")
    else:
        code, frames, columns, quads, gpu_bytes = map(int, final.groups())
        report["world"] = dict(code=code, frames=frames, columns=columns,
                               quads=quads, gpu_bytes=gpu_bytes)
        if code != 0 or frames < 600 or columns != (2 * radius + 1) ** 2 or min(quads, gpu_bytes) <= 0:
            failures.append("Incomplete configured world or frame count")
    error_pattern = re.compile(
        r"rhi_validation severity=error|Validation Error|rmlui severity=error|"
        r"rml_ui\w*_contract=failed|rmlui image failed|world_validation.*failed", re.I
    )
    report["errors"] = [line for line in text.splitlines() if error_pattern.search(line)]
    report["warnings"] = [line for line in text.splitlines() if re.search(r"warn(?:ing)?", line, re.I)]
    report["device_attempt_errors"] = [line for line in text.splitlines()
                                        if "rhi_device_attempt severity=error" in line]
    if report["errors"]:
        failures.append("Runtime graphics/UI validation errors were reported")
    # Warnings remain visible in the report and status, including device retries.
    if not capture.is_file() or capture.stat().st_size < 1024:
        failures.append("Presented GPU screenshot is missing or too small")
    else:
        header = capture.read_bytes()[:26]
        width, height = struct.unpack_from("<ii", header, 18)
        report["capture"] = {"path": str(capture), "sha256": sha256(capture),
                             "width": width, "height": abs(height)}
        if header[:2] != b"BM" or (width, abs(height)) != (1280, 720):
            failures.append("GPU screenshot is not a 1280x720 BMP")
    temporal = Path(str(capture) + ".temporal.json")
    if not temporal.is_file():
        failures.append("Missing temporal capture metadata for actual Native AA mode")
    else:
        data = json.loads(temporal.read_text(encoding="utf-8"))
        report["temporal"] = {"path": str(temporal), "sha256": sha256(temporal),
                              "mode": data.get("mode"),
                              "render_width": data.get("render_width"),
                              "render_height": data.get("render_height")}
        if data.get("mode") != 1 or (data.get("render_width"), data.get("render_height")) != (1280, 720):
            failures.append("Actual FSR mode/resolution does not match Native AA")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--api", choices=("dx12", "vulkan"), required=True)
    parser.add_argument("--radius", type=int, choices=(4, 32), default=4)
    parser.add_argument("--hidden", action="store_true", help="Use the client's internal hidden validation window")
    parser.add_argument("--vulkan-icd", type=Path, help="Explicit Vulkan driver manifest for qualification")
    parser.add_argument("--timeout-seconds", type=int, default=300)
    args = parser.parse_args()
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("--timeout-seconds must be between 1 and 900")
    bundle, evidence, repo = (p.resolve() for p in (args.bundle, args.evidence_root, args.repo_root))
    if evidence.is_relative_to(bundle) or bundle.is_relative_to(evidence):
        parser.error("Evidence and bundle must be separate directory trees")
    if evidence.exists():
        parser.error("Evidence directory already exists; choose a fresh directory")
    evidence.mkdir(parents=True)
    report = {"status": "failed", "api_requested": args.api, "radius": args.radius,
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "bundle": str(bundle), "evidence": str(evidence), "failures": [],
              "warnings": [], "errors": []}
    start_time = time.monotonic()
    log = evidence / "client.log"
    suffix = ".exe" if os.name == "nt" else ""
    executable = bundle / f"Octaryn.Client{suffix}"
    report["host_platform"] = platform.platform()
    report["hidden_window"] = args.hidden
    try:
        if platform.system() not in ("Windows", "Linux"):
            raise RuntimeError("This verifier requires native Windows or Linux")
        if os.name != "nt" and args.api != "vulkan":
            raise RuntimeError("Linux qualification requires Vulkan")
        if not executable.is_file() or not (bundle / f"server/Octaryn.Server{suffix}").is_file():
            raise RuntimeError("Packaged client or local server executable missing")
        report["client_sha256"] = sha256(executable)
        world = evidence / "world"
        world.mkdir()
        settings = evidence / "settings.json"
        settings.write_text(json.dumps({"windowWidth": 1280, "windowHeight": 720,
                            "fullscreen": False, "renderDistance": args.radius,
                            "upscalerMode": 1, "fsrDynamicResolution": 0}), encoding="utf-8")
        report["initial_settings_sha256"] = sha256(settings)
        environment = {key: value for key, value in os.environ.items()
                       if not key.upper().startswith(("OCTARYN_", "VK_"))}
        appdata = evidence / "appdata"
        appdata.mkdir()
        capture = evidence / "frame.bmp"
        environment.update({
            "APPDATA": str(appdata), "LOCALAPPDATA": str(appdata),
            "OCTARYN_CLIENT_WORLD_PATH": str(world),
            "OCTARYN_CLIENT_SETTINGS_PATH": str(settings),
            "OCTARYN_CLIENT_LIGHTING_PATH": str(evidence / "lighting.json"),
            "OCTARYN_CLIENT_INVENTORY_PATH": str(evidence / "inventory.json"),
            "OCTARYN_CLIENT_GRAPHICS_API": args.api,
            "OCTARYN_CLIENT_RHI_VALIDATION": "1",
            "OCTARYN_CLIENT_CAPTURE_PATH": str(capture),
            "OCTARYN_CLIENT_CAPTURE_TEMPORAL": "1",
            "OCTARYN_CLIENT_PROFILE_PATH": str(evidence / "world-profile.csv"),
        })
        if os.name != "nt":
            environment.update(SDL_VIDEO_DRIVER="x11", XDG_CONFIG_HOME=str(appdata))
        if args.api == "vulkan":
            layers = (repo / "build/dependencies/vulkan-validation" if os.name == "nt"
                      else Path("/usr/share/vulkan/explicit_layer.d"))
            required = ["VkLayer_khronos_validation.json"]
            if os.name == "nt":
                required.append("vk_layer_settings.txt")
            for name in required:
                if not (layers / name).is_file():
                    raise RuntimeError(f"Required Vulkan validation configuration missing: {layers / name}")
            environment.update(VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation",
                               VK_LAYER_PATH=str(layers), VK_LAYER_SETTINGS_PATH=str(layers))
            report["vulkan_validation_path"] = str(layers)
            if args.vulkan_icd:
                icd = args.vulkan_icd.resolve(strict=True)
                environment["VK_DRIVER_FILES"] = str(icd)
                report["vulkan_driver_manifest"] = {"path": str(icd), "sha256": sha256(icd)}
        command = [str(executable), "--frames", "600", "--render-distance", str(args.radius), "--validate-ui"]
        if args.hidden:
            command.append("--benchmark-hidden")
        report["command"] = command
        print(f"package_validation running api={args.api} radius={args.radius} evidence={evidence}", flush=True)
        with log.open("xb") as stream:
            process = subprocess.Popen(command, cwd=evidence, env=environment,
                                       stdout=stream, stderr=subprocess.STDOUT)
            report["client_pid"] = process.pid
            try:
                result = process.wait(timeout=args.timeout_seconds)
            except subprocess.TimeoutExpired:
                shutdown(process, world)
                report["failures"].append(f"Client exceeded {args.timeout_seconds}-second execution limit")
                result = process.returncode
        report["exit_code"] = result
        inspect(log.read_text(encoding="utf-8", errors="replace"), result,
                capture, args.api, args.radius, report)
        if sha256(executable) != report["client_sha256"]:
            report["failures"].append("Client executable changed during validation")
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        report["failures"].append(str(error))
    finally:
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        report["elapsed_seconds"] = round(time.monotonic() - start_time, 3)
        if log.is_file():
            report["log"] = {"path": str(log), "sha256": sha256(log)}
        if not report["failures"]:
            report["status"] = "passed_with_warnings" if report["warnings"] or report.get("device_attempt_errors") else "passed"
        (evidence / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 1 if report["failures"] else 0


if __name__ == "__main__":
    sys.exit(main())
