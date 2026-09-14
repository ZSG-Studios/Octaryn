"""Packaged nine-phase temporal qualification through production domain APIs."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

from validate_rhi_client_diagnostic import inspect_result

PHASES = [(0, 1280, 720), (1, 1280, 720), (2, 1280, 720), (3, 1280, 720),
          (4, 1280, 720), (5, 1280, 720), (2, 1280, 720), (2, 960, 540), (2, 1280, 720)]
RATIOS = [1, 1, 1.5, 1.7, 2, 3]
API_NAMES = {"dx12": "D3D12", "vulkan": "Vulkan", "metal": "Metal"}
PHASE_PATTERN = re.compile(
    r"temporal_validation phase=(\d+) mode=(\d+) window=(\d+)x(\d+) "
    r"render=(\d+)x(\d+) output=(\d+)x(\d+) ui=(\d+)x(\d+) resets=(\d+) successful_frames=(\d+)")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def inspect_log(returncode, text, backend, frame_count):
    frames, columns, quads = inspect_result(returncode, text, API_NAMES[backend], minimum_frames=108)
    require(columns == 81, "Temporal qualification must retain the complete radius4 window")
    require(text.count("world_validation core=required") == 1, "Required native graphics validation was not enabled")
    counts = re.findall(r"world_frames count=(\d+) mutable_targets=per_slot", text)
    require(counts == [str(frame_count)], "Actual graphics frame slots differ from requested count")
    require("rml_ui renderer=slang-rhi frame=" in text, "No production RmlUi geometry submission")
    require(not re.search(r"rmlui severity=(?:error|warning)|rml_ui\w*=failed|rmlui image failed|"
                         r"temporal_validation=failed|world_fsr2\w*=failed|fsr2.*(?:failed|error)", text, re.I),
            "UI/temporal validation reported a failure or warning")
    matches = list(PHASE_PATTERN.finditer(text))
    require(len(matches) == len(PHASES), "Exactly nine successful temporal phase reports are required")
    phases = []
    for index, (match, expected) in enumerate(zip(matches, PHASES)):
        phase, mode, ww, wh, rw, rh, ow, oh, uw, uh, resets, successful = map(int, match.groups())
        require((phase, mode, ww, wh) == (index, *expected), f"Incorrect mode/window order in phase{index}")
        require(ow > 0 and oh > 0 and ow * wh == oh * ww, f"Invalid output aspect in phase{index}")
        expected_render = tuple(math.floor(value / RATIOS[mode] + .5) for value in (ow, oh))
        require((rw, rh) == expected_render and (uw, uh) == (ow, oh),
                f"Render scale or native RmlUi dimensions differ in phase{index}")
        require(successful == 12 if index < 8 else successful >= 12,
                f"Phase{index} lacks twelve submitted resident frames")
        if phases:
            require(resets == phases[-1]["resets"] + 1, f"Phase{index} reset count must advance exactly once")
            require(ow * 1280 == phases[0]["output"][0] * ww and
                    oh * 720 == phases[0]["output"][1] * wh,
                    "Native output pixel density changed during qualification")
        phases.append(dict(phase=phase, mode=mode, window=[ww, wh], render=[rw, rh], output=[ow, oh],
                           ui=[uw, uh], resets=resets, successful_frames=successful))
    completion = list(re.finditer(
        r"temporal_validation=passed modes=6 phases=9 successful_frames=(\d+) final_capture=1 os_events_injected=0", text))
    require(len(completion) == 1 and completion[0].start() > matches[-1].start(), "Missing final temporal completion")
    total = sum(phase["successful_frames"] for phase in phases)
    # OpenWorld breaks on completion before incrementing its final resident frame.
    require(int(completion[0].group(1)) == total and frames + 1 >= total, "Successful phase/frame totals disagree")
    fsr = re.findall(r"world_fsr2 version=2\.2\.1 mode=(\d+) render=(\d+)x(\d+) output=(\d+)x(\d+)", text)
    expected_fsr = [(p["mode"], *p["render"], *p["output"]) for p in phases[1:]]
    require([tuple(map(int, row)) for row in fsr] == expected_fsr,
            "Actual FSR2 resource creation does not match all eight enabled-mode phases")
    captures = list(re.finditer(r"world_capture frame=(\d+) columns=(\d+) nonclear_pixels=(\d+)", text))
    require(len(captures) == 1, "Exactly one final presented GPU capture is required")
    capture = captures[0]
    require(matches[7].start() < capture.start() < matches[8].start() and
            int(capture.group(1)) >= 120 and int(capture.group(2)) == 81 and int(capture.group(3)) > 0,
            "Capture is incomplete or occurred before the final resize phase")
    return dict(frames=frames, columns=columns, quads=quads, phases=phases,
                successful_frames=total, native_validation="core_required", diagnostics="no_reported_warnings_or_errors")


def inspect_files(case, result):
    image = (case / "frame.bmp").read_bytes()
    require(len(image) > 1024 and image[:2] == b"BM", "Missing or invalid final GPU BMP")
    width, height = struct.unpack_from("<ii", image, 18)
    require([width, abs(height)] == result["phases"][-1]["output"], "Capture dimensions differ from final native output")
    server_log = case / "world/logs/server/local-session.log"
    server = server_log.read_text(encoding="utf-8", errors="replace")
    require("octaryn_server_shutdown=1" in server, "Isolated server did not shut down gracefully")
    require("Unhandled exception" not in server, "Isolated authority reported an unhandled exception")
    result.update(capture=str(case / "frame.bmp"), server_log=str(server_log))


def stop_owned(process, world):
    # Only this runner's child and this isolated world's server are affected.
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
            process.wait(timeout=5)


def run(args):
    bundle = args.client_bundle_root.resolve()
    suffix = ".exe" if os.name == "nt" else ""
    executable = bundle / f"Octaryn.Client{suffix}"
    require(executable.is_file() and (bundle / "server" / f"Octaryn.Server{suffix}").is_file(),
            "Complete packaged client and sibling server are required")
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix="temporal-", dir=args.evidence_root.resolve()))
    world = case / "world"
    world.mkdir()
    settings = {"version": 8, "windowWidth": 1280, "windowHeight": 720, "fullscreen": False,
                "renderDistance": 4, "upscalerMode": 0, "presentModeIndex": 0}
    (case / "settings.json").write_text(json.dumps(settings), encoding="utf-8")
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("OCTARYN_CLIENT_", "OCTARYN_SERVER_"))}
    overrides = {
        "OCTARYN_CLIENT_WORLD_PATH": str(world),
        "OCTARYN_CLIENT_SETTINGS_PATH": str(case / "settings.json"),
        "OCTARYN_CLIENT_LIGHTING_PATH": str(case / "lighting.json"),
        "OCTARYN_CLIENT_INVENTORY_PATH": str(case / "inventory.json"),
        "OCTARYN_CLIENT_CAPTURE_PATH": str(case / "frame.bmp"),
        "OCTARYN_CLIENT_PROFILE_PATH": str(case / "world-profile.csv"),
        "OCTARYN_CLIENT_RHI_VALIDATION": "1",
        "OCTARYN_CLIENT_GRAPHICS_API": args.backend,
        "OCTARYN_CLIENT_FRAMES_IN_FLIGHT": str(args.frames_in_flight),
    }
    if args.backend == "vulkan":
        overrides["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
        if os.name == "nt":
            layers = Path(__file__).resolve().parents[2] / "build/dependencies/vulkan-validation"
            require((layers / "VkLayer_khronos_validation.json").is_file() and
                    (layers / "vk_layer_settings.txt").is_file(), "Pinned Vulkan validation layer/settings are missing")
            overrides.update(VK_LAYER_PATH=str(layers), VK_LAYER_SETTINGS_PATH=str(layers))
    # Deliberately absent: OCTARYN_CLIENT_UPSCALER. The real settings value must drive every phase.
    environment.update(overrides)
    command = [str(executable), "--validate-temporal", "--benchmark-hidden"]
    report = dict(status="running", backend=args.backend, frames_in_flight=args.frames_in_flight,
                  command=command, environment_overrides=overrides, evidence=str(case),
                  client_sha256=hashlib.sha256(executable.read_bytes()).hexdigest())
    report_path = case / "result.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"temporal_validation_started evidence={case}", flush=True)
    try:
        with (case / "client.log").open("wb") as output:
            process = subprocess.Popen(command, cwd=case, env=environment, stdout=output, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=540)
            except subprocess.TimeoutExpired:
                stop_owned(process, world)
                raise RuntimeError("Temporal validation exceeded540 seconds")
            except KeyboardInterrupt:
                stop_owned(process, world)
                raise
        text = (case / "client.log").read_text(encoding="utf-8", errors="replace")
        result = inspect_log(code, text, args.backend, args.frames_in_flight)
        inspect_files(case, result)
        report.update(result, status="passed")
    except BaseException as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"temporal_packaged=passed backend={args.backend} frames_in_flight={args.frames_in_flight} evidence={case}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-bundle-root", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--backend", required=True, choices=tuple(API_NAMES))
    parser.add_argument("--frames-in-flight", required=True, type=int, choices=(1, 2))
    run(parser.parse_args())


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(f"temporal_packaged=failed: {error}", file=sys.stderr)
        sys.exit(1)
