"""One real packaged, full-detail presentation benchmark; no OS input injection.

Compare result.json files only when binary/content hashes, geometry and camera
match. GPU timings are measured query rows, never inferred from frame timings.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import tempfile
import time

UPSCALERS = ("off", "native", "quality", "balanced", "performance", "ultra-performance")
WIDTH, HEIGHT, RADIUS, COLUMNS = 2560, 1440, 32, 4225


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def content_digest(bundle):
    # Include runtime code and content, not logs/caches which change during runs.
    extensions = {".exe", ".dll", ".so", ".dylib", ".slang", ".json", ".png",
                  ".gltf", ".bin", ".rml", ".rcss", ".ttf", ".otf"}
    entries = []
    for path in sorted(bundle.rglob("*")):
        relative = path.relative_to(bundle)
        if path.is_file() and path.suffix.lower() in extensions and not any(
                part.lower() in {"logs", "saves", "cache", "runtime"} for part in relative.parts):
            entries.append((relative.as_posix(), digest(path)))
    return {"sha256": hashlib.sha256(json.dumps(entries).encode()).hexdigest(),
            "files": len(entries)}


def numeric(row, field):
    value = float(row[field])
    require(math.isfinite(value), f"Nonfinite profile field: {field}")
    return value


def read_csv(path):
    with path.open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.DictReader(stream))
    require(rows and all(None not in row for row in rows), f"Missing or malformed profile: {path}")
    frames = [int(row["frame"]) for row in rows]
    require(all(b > a for a, b in zip(frames, frames[1:])), f"Unordered/duplicate frames: {path}")
    return rows


def write_csv(path, rows):
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def distribution(values):
    ordered = sorted(values)
    def percentile(p):
        return ordered[min(len(ordered)-1, math.ceil(p*len(ordered))-1)]
    return {"mean": statistics.fmean(ordered), "p50": percentile(.5),
            "p95": percentile(.95), "p99": percentile(.99), "max": ordered[-1]}


def inspect_run(case, code, args):
    text = (case / "client.log").read_text(encoding="utf-8", errors="replace")
    require(code == 0, f"Client exited with {code}")
    require(not re.search(r"rhi_validation severity=(?:error|warning)|Validation Error:|"
                          r"Validation Warning:|(?:failed|failure|timed out)", text, re.IGNORECASE),
            "Client reported a failure or graphics warning; inspect client.log")
    api = "D3D12" if args.backend == "dx12" else "Vulkan"
    device = re.search(r"world_device backend=slang_rhi api=(\S+) adapter=([^\r\n]+)", text)
    require(device and device[1] == api, "Requested graphics API was not used")
    expected_batch = "bindless" if args.batch == "required" else "legacy"
    require(re.search(rf"world_batch mode={expected_batch}\b", text), "Requested batching path was not used")
    if args.batch == "off":
        require("world_batch mode=legacy reason=disabled required=0" in text,
                "Legacy drawing was a fallback instead of explicitly disabled batching")
    fsr = re.findall(r"world_fsr2 version=2\.2\.1 mode=(\d+) render=(\d+)x(\d+) output=(\d+)x(\d+)", text)
    mode = UPSCALERS.index(args.upscaler)
    require((not fsr) if mode == 0 else bool(fsr) and all(
        int(row[0]) == mode and tuple(map(int, row[3:])) == (WIDTH, HEIGHT) for row in fsr),
        "Actual upscaler/output does not match requested configuration")
    require("world_benchmark resident=complete meshes=complete warmup_seconds=5" in text,
            "Full residency/mesh completion was never reached")
    start = re.search(r"world_benchmark measurement=start frame=(\d+)", text)
    require(start, "Missing post-warmup measurement marker; rebuild the packaged client")
    final = re.search(r"open_world_exit code=0 frames=(\d+) columns=(\d+) quads=(\d+) gpu_bytes=(\d+)", text)
    metrics = re.search(r"world_profile average_ms=([\d.]+) low_1pct_fps=([\d.]+) "
                        r"worst_ms=([\d.]+) samples=(\d+)", text)
    require(final and metrics and int(final[2]) == COLUMNS, "Missing normal full-world benchmark completion")
    require(start.start() < metrics.start() < final.start(), "Invalid benchmark completion ordering")
    first_frame, quads = int(start[1]), int(final[3])
    cpu = [row for row in read_csv(case / "world-profile.csv") if int(row["frame"]) > first_frame]
    gpu = [row for row in read_csv(case / "gpu-profile.csv") if int(row["frame"]) >= first_frame]
    require(len(cpu) >= max(2, math.floor(args.seconds)-2) and len(gpu) >= 2,
            "Insufficient post-warmup CPU/GPU samples")
    span = numeric(cpu[-1], "time_seconds")-numeric(cpu[0], "time_seconds")
    require(span >= args.seconds-2.5, "Measured interval ended too early")
    for row in cpu:
        require(int(row["columns"]) == COLUMNS and int(row["pending_meshes"]) == 0
                and int(row["quads"]) == quads, "CPU measurement contains incomplete/changing geometry")
        require((int(row["width"]), int(row["height"])) == (WIDTH, HEIGHT), "Output size is not 1440p")
    for row in gpu:
        require(int(row["columns"]) == COLUMNS and int(row["quads"]) == quads,
                "GPU measurement contains incomplete/changing geometry")
        require(int(row["world_batch"]) == (args.batch == "required"), "GPU rows used another batch path")
        require((int(row["width"]), int(row["height"])) == (WIDTH, HEIGHT), "GPU viewport is not 1440p")
    gpu_frames = {int(row["frame"]): row for row in gpu}
    # Renderer records zero-based frames before increment; app CPU records after increment.
    require(all(int(row["frame"])-1 in gpu_frames for row in cpu), "CPU/GPU measured frame IDs do not join")
    for row in cpu:
        other = gpu_frames[int(row["frame"])-1]
        require(all(row[key] == other[key] for key in ("quads", "columns", "drawn_columns", "drawn_quads")),
                "Matched CPU/GPU rows disagree about exact geometry")
    write_csv(case / "measured-cpu.csv", cpu)
    write_csv(case / "measured-gpu.csv", gpu)
    average, low, worst, samples = map(float, metrics.groups())
    require(args.seconds-.5 <= average*samples/1000 <= args.seconds+.5,
            "Frame metrics did not cover the complete requested interval (check duplicate warmup)")
    require(average > 0 and low > 0 and samples > 0, "Invalid measured frame metrics")
    phases = {key: distribution([numeric(row, key) for row in gpu])
              for key in gpu[0] if key.endswith("_ms")}
    generation = json.loads((case / "world/world_generation.json").read_text(encoding="utf-8"))
    require(generation == {"version": 1, "generator": "octaryn.basegame", "revision": 2, "seed": 1337, "mode": 0},
            "World generation differs from the matched default terrain contract")
    return {"device": device[2], "api": api, "measurement_start_frame": first_frame,
            "world_generation": generation, "cpu_to_gpu_frame_offset": -1,
            "frame_metrics": {"source": "post-warmup world_profile stdout", "average_ms": average,
                              "average_fps": 1000/average, "low_1pct_fps": low,
                              "worst_ms": worst, "samples": int(samples)},
            "cpu_rows": len(cpu), "gpu_rows": len(gpu), "cpu_gpu_joined_rows": len(cpu),
            "cpu_sample_span_seconds": span, "columns": COLUMNS, "quads": quads,
            "gpu_bytes": int(final[4]), "fsr_records": fsr,
            "camera_positions": sorted({(row["eye_x"], row["eye_y"], row["eye_z"]) for row in cpu}),
            "drawn_columns": sorted({int(row["drawn_columns"]) for row in gpu}),
            "drawn_quads": sorted({int(row["drawn_quads"]) for row in gpu}),
            "draw_commands": distribution([numeric(row, "world_draw_commands") for row in gpu]),
            "phase_timings_ms": phases}


def stop_owned(process, case):
    runtime = case / "world/runtime"
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-bundle-root", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--backend", required=True, choices=("dx12", "vulkan"))
    parser.add_argument("--upscaler", choices=UPSCALERS, default="off")
    parser.add_argument("--batch", choices=("off", "required"), default="required")
    parser.add_argument("--seconds", type=float, default=15)
    args = parser.parse_args()
    require(math.isfinite(args.seconds) and 5 <= args.seconds <= 120, "--seconds must be 5..120")
    bundle = args.client_bundle_root.resolve()
    executable = bundle / ("Octaryn.Client.exe" if os.name == "nt" else "Octaryn.Client")
    require(executable.is_file(), f"Packaged client missing: {executable}")
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f"presentation-{args.backend}-{args.upscaler}-{args.batch}-",
                                 dir=args.evidence_root.resolve()))
    (case / "world").mkdir()
    settings = {"version": 8, "windowWidth": WIDTH, "windowHeight": HEIGHT, "fullscreen": False,
                "renderDistance": RADIUS, "upscalerMode": UPSCALERS.index(args.upscaler),
                "presentModeIndex": 0, "fogEnabled": False, "pbrEnabled": True, "pomEnabled": True,
                "cloudsEnabled": True, "starsEnabled": True, "skyGradientEnabled": True,
                "sunEnabled": True, "moonEnabled": True}
    lighting = {"version": 1, "ambient_strength": .82, "sun_strength": 1,
                "sun_fallback_strength": 1, "fog_distance": 256, "skylight_floor": .08}
    for name, data in (("settings", settings), ("lighting", lighting)):
        (case / f"{name}.json").write_text(json.dumps(data, indent=2), encoding="utf-8")
    # Never inherit validation, capture, culling, timing, transport or saved-world overrides.
    removed = [key for key in os.environ if key.startswith("OCTARYN_") or key.startswith("VK_LAYER")
               or key in {"VK_INSTANCE_LAYERS", "VK_LOADER_LAYERS_ENABLE"}]
    environment = {key: value for key, value in os.environ.items() if key not in removed}
    overrides = {f"OCTARYN_CLIENT_{key}_PATH": str(case / value) for key, value in {
        "WORLD": "world", "SETTINGS": "settings.json", "LIGHTING": "lighting.json",
        "INVENTORY": "inventory.json", "PROFILE": "world-profile.csv", "GPU_PROFILE": "gpu-profile.csv"}.items()}
    overrides.update(OCTARYN_CLIENT_GRAPHICS_API=args.backend, OCTARYN_CLIENT_UPSCALER=args.upscaler,
                     OCTARYN_CLIENT_WORLD_BATCH=args.batch, OCTARYN_CLIENT_FRAMES_IN_FLIGHT="2")
    environment.update(overrides)
    command = [str(executable), "--benchmark-settings", "--benchmark-hidden",
               "--benchmark-seconds", str(args.seconds), "--render-distance", str(RADIUS)]
    report = {"status": "running", "backend": args.backend, "upscaler": args.upscaler, "batch": args.batch,
              "seconds": args.seconds, "warmup_seconds": 5, "timeout_seconds": 300,
              "width": WIDTH, "height": HEIGHT, "radius": RADIUS, "no_lod": True,
              "validation_enabled": False, "command": command, "settings": settings, "lighting": lighting,
              "environment_overrides": overrides, "removed_environment_keys": sorted(removed),
              "client_sha256": digest(executable), "bundle_content": content_digest(bundle),
              "evidence": str(case), "comparison_requirement":
              "Match executable/content hashes, geometry, camera and config except intentional API/batch/upscaler changes."}
    report_path = case / "result.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"presentation_benchmark_started evidence={case}", flush=True)
    begin = time.monotonic()
    try:
        with (case / "client.log").open("wb") as output:
            process = subprocess.Popen(command, cwd=case, env=environment, stdout=output, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=300)
            except subprocess.TimeoutExpired:
                stop_owned(process, case)
                raise RuntimeError("Benchmark exceeded 300 seconds; partial evidence retained")
        report.update(inspect_run(case, code, args))
        require(digest(executable) == report["client_sha256"], "Packaged executable changed during measurement")
        require(content_digest(bundle) == report["bundle_content"], "Packaged runtime/content changed during measurement")
        report["status"] = "passed"
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        report["wall_seconds"] = time.monotonic()-begin
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"presentation_benchmark=passed evidence={case}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(f"presentation_benchmark=failed: {error}", file=sys.stderr)
        sys.exit(1)
