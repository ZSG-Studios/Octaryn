"""Measure real full-detail chunk loading along an explicit linear camera fixture.

This changes the render camera and client stream request only; it does not inject
input or simulate authoritative player travel. Startup and moving samples stay
separate. Compare configurations and route; changed binaries are recorded.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import subprocess
import struct
import sys
import tempfile
import time

from benchmark_presentation import (UPSCALERS, content_digest, digest, distribution,
                                    numeric, read_csv, require, stop_owned, write_csv)


def summarize(rows):
    if not rows:
        return {"samples": 0}
    phases = ("frame_ms", "session_ms", "stream_mesh_ms", "render_ms", "ui_ms", "events_ms", "profile_ms")
    timings = {key: distribution([numeric(row, key) for row in rows]) for key in phases}
    timings["unattributed_ms"] = distribution([max(0, numeric(row, "frame_ms")-
        sum(numeric(row, key) for key in phases if key != "frame_ms")) for row in rows])
    return {"samples": len(rows), "phase_timings_ms": timings,
        "frames_over_16_67_ms": sum(numeric(row, "frame_ms") > 1000/60 for row in rows),
        "frames_over_33_33_ms": sum(numeric(row, "frame_ms") > 1000/30 for row in rows),
        "frames_over_50_ms": sum(numeric(row, "frame_ms") > 50 for row in rows)}


def inspect_run(case, code, args):
    log = (case / "client.log").read_text(encoding="utf-8", errors="replace")
    require(code == 0, f"Client exited with {code}")
    require(not re.search(r"rhi_validation severity=(?:error|warning)|Validation Error:|"
                          r"Validation Warning:|(?:failed|failure|timed out)", log, re.I),
            "Client reported a failure or graphics warning; inspect client.log")
    api = "D3D12" if args.backend == "dx12" else "Vulkan"
    device = re.search(r"world_device backend=slang_rhi api=(\S+) adapter=([^\r\n]+)", log)
    require(device and device[1] == api, "Requested backend was not used")
    require("world_stream_benchmark fixture=camera_linear" in log, "Missing explicit motion fixture")
    require("world_benchmark resident=complete meshes=complete warmup_seconds=5" in log,
            "Initial residency/mesh warmup did not complete")
    require("world_batch mode=bindless" in log, "Required world batching was not used")
    marker = re.search(r"world_benchmark measurement=start frame=(\d+)", log)
    require(marker, "Missing measurement start")
    final = re.search(r"open_world_exit code=0 frames=(\d+) columns=(\d+) quads=(\d+) gpu_bytes=(\d+)", log)
    require(final, "Missing normal completion")
    settled = re.search(r"world_stream_benchmark settled=complete center=(-?\d+),(-?\d+) camera=([\d.-]+),([\d.-]+),([\d.-]+) quads=(\d+) gpu_bytes=(\d+)", log)
    require(settled, "Final stream window did not settle")
    rows = read_csv(case / "stream-profile.csv")
    measured = [row for row in rows if row["phase"] == "moving" and int(row["frame"]) > int(marker[1])]
    startup = [row for row in rows if row["phase"] == "startup"]
    settle = [row for row in rows if row["phase"] == "settle"]
    require(len(measured) >= 60, "Too few moving frames")
    require(all(int(b["frame"]) == int(a["frame"])+1 for a, b in zip(measured, measured[1:])),
            "Moving profile omitted frames")
    span = numeric(measured[-1], "time_seconds")-numeric(measured[0], "time_seconds")
    require(span >= args.seconds-.5, "Moving interval ended early")
    expected = (2*args.radius+1)**2
    for row in measured:
        require(0 < int(row["columns"]) <= expected and int(row["expected_columns"]) == expected,
                "Column residency is empty or exceeds the requested window")
        require(0 <= int(row["drawn_columns"]) <= int(row["columns"]), "Invalid visible column count")
        require(0 < int(row["drawn_quads"]) <= int(row["quads"]), "Empty or invalid rendered geometry")
        require(int(row["center_x"]) == math.floor(numeric(row, "camera_x")/32) and
                int(row["center_z"]) == math.floor(numeric(row, "camera_z")/32),
                "Stream center does not follow the rendered camera")
    travel = numeric(measured[0], "camera_z")-numeric(measured[-1], "camera_z")
    require(abs(travel-args.speed*span) < .1, "Camera did not follow the timed linear route")
    centers = {(int(row["center_x"]), int(row["center_z"])) for row in measured}
    require(len(centers) >= max(2, math.floor(args.speed*span/32)), "Insufficient moving-center transitions")
    require(settle and int(settle[-1]["columns"]) == expected and int(settle[-1]["pending_meshes"]) == 0,
            "Final geometry is incomplete")
    require(abs(numeric(startup[-1], "camera_z")-float(settled[5])-args.seconds*args.speed) < .01,
            "Final camera endpoint differs from the exact requested distance")
    snapshot = (case / "world/runtime/chunk_stream.json.bin").read_bytes()
    require(snapshot[:8] == b"OCSTRM01" and struct.unpack_from("<I", snapshot, 8)[0] == 2,
            "Unexpected server stream snapshot format")
    center_x, center_z, radius = struct.unpack_from("<iiI", snapshot, 20)
    require((center_x, center_z, radius) == (int(settled[1]), int(settled[2]), args.radius),
            "Authoritative server stream did not reach the rendered final center")
    count = struct.unpack_from("<I", snapshot, 112)[0]
    coordinates = {struct.unpack_from("<ii", snapshot, 120+24*index) for index in range(count)}
    requested = {(x, z) for x in range(center_x-radius, center_x+radius+1)
                 for z in range(center_z-radius, center_z+radius+1)}
    require(count == expected and coordinates == requested, "Final snapshot identities differ from the full requested window")
    loading = [row for row in measured if int(row["loading"])]
    require(loading, "Movement did not exercise chunk loading")
    require(any(numeric(row, "stream_mesh_ms") > 0 for row in loading), "No measured stream work")
    fsr = re.findall(r"world_fsr2 version=2\.2\.1 mode=(\d+) render=(\d+)x(\d+) output=(\d+)x(\d+)", log)
    mode = UPSCALERS.index(args.upscaler)
    require((not fsr) if mode == 0 else bool(fsr) and all(int(item[0]) == mode for item in fsr),
            "Requested upscaler was not used")
    gpu = [row for row in read_csv(case / "gpu-profile.csv")
           if int(marker[1]) <= int(row["frame"]) < int(measured[-1]["frame"])]
    gpu_by_frame = {int(row["frame"]): row for row in gpu}
    joined = [row for row in measured if int(row["frame"])-1 in gpu_by_frame]
    require(len(joined) >= len(measured)-3, "GPU measurements do not cover moving CPU frames")
    for row in joined:
        other = gpu_by_frame[int(row["frame"])-1]
        require(all(row[key] == other[key] for key in ("columns", "quads", "drawn_columns", "drawn_quads")),
                "Matched CPU/GPU frames disagree about geometry")
        require((int(other["width"]), int(other["height"])) == (args.width, args.height),
                "GPU output size differs from requested resolution")
    write_csv(case / "measured-stream.csv", measured)
    generation = json.loads((case / "world/world_generation.json").read_text(encoding="utf-8"))
    require(generation == {"version": 1, "generator": "octaryn.basegame", "revision": 2, "seed": 1337, "mode": 0},
            "Unexpected world generation contract")
    return {"api": api, "device": device[2], "world_generation": generation,
            "measured_span_seconds": span, "camera_distance_metres": travel,
            "centers": sorted(centers), "moving": summarize(measured),
            "moving_loading": summarize(loading),
            "moving_resident": summarize([row for row in measured if not int(row["loading"])]),
            "startup_and_warmup": summarize(startup), "fsr_records": fsr,
            "settle": summarize(settle), "final_center": [center_x, center_z],
            "final_camera": [float(settled[index]) for index in (3, 4, 5)],
            "final_columns": count, "final_quads": int(settled[6]), "final_gpu_bytes": int(settled[7]),
            "final_snapshot_identities": "complete exact window",
            "columns_min": min(int(row["columns"]) for row in measured),
            "columns_max": max(int(row["columns"]) for row in measured),
            "pending_meshes_max": max(int(row["pending_meshes"]) for row in measured),
            "temporal_resets": sorted({int(row["temporal_resets"]) for row in measured}),
            "gpu_bytes_max": max(int(row["gpu_bytes"]) for row in measured),
            "quads_range": [min(int(row["quads"]) for row in measured), max(int(row["quads"]) for row in measured)],
            "cpu_gpu_joined_rows": len(joined), "gpu_phase_timings_ms": {
                key: distribution([numeric(row, key) for row in gpu]) for key in gpu[0] if key.endswith("_ms")}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-bundle-root", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--backend", required=True, choices=("dx12", "vulkan"))
    parser.add_argument("--upscaler", choices=UPSCALERS, default="quality")
    parser.add_argument("--seconds", type=float, default=15)
    parser.add_argument("--speed", type=float, default=32)
    parser.add_argument("--radius", type=int, choices=(4, 8, 12, 16, 20, 24, 32), default=32)
    parser.add_argument("--width", type=int, default=2560)
    parser.add_argument("--height", type=int, default=1440)
    args = parser.parse_args()
    require(math.isfinite(args.seconds) and 5 <= args.seconds <= 120, "--seconds must be 5..120")
    require(math.isfinite(args.speed) and 1 <= args.speed <= 120, "--speed must be 1..120")
    require(args.seconds*args.speed >= 64, "Route must travel at least two columns")
    require(640 <= args.width <= 7680 and 480 <= args.height <= 4320, "Invalid output size")
    bundle = args.client_bundle_root.resolve()
    executable = bundle / ("Octaryn.Client.exe" if os.name == "nt" else "Octaryn.Client")
    require(executable.is_file(), f"Packaged client missing: {executable}")
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f"streaming-{args.backend}-{args.upscaler}-", dir=args.evidence_root.resolve()))
    (case / "world").mkdir()
    settings = {"version": 8, "windowWidth": args.width, "windowHeight": args.height, "fullscreen": False,
                "renderDistance": args.radius, "upscalerMode": UPSCALERS.index(args.upscaler),
                "presentModeIndex": 0, "fogEnabled": False, "pbrEnabled": True, "pomEnabled": True,
                "cloudsEnabled": True, "starsEnabled": True, "skyGradientEnabled": True,
                "sunEnabled": True, "moonEnabled": True}
    lighting = {"version": 1, "ambient_strength": .82, "sun_strength": 1,
                "sun_fallback_strength": 1, "fog_distance": 256, "skylight_floor": .08}
    for name, data in (("settings", settings), ("lighting", lighting)):
        (case / f"{name}.json").write_text(json.dumps(data, indent=2), encoding="utf-8")
    removed = [key for key in os.environ if key.startswith("OCTARYN_") or key.startswith("VK_LAYER")
               or key in {"VK_INSTANCE_LAYERS", "VK_LOADER_LAYERS_ENABLE"}]
    environment = {key: value for key, value in os.environ.items() if key not in removed}
    overrides = {f"OCTARYN_CLIENT_{key}_PATH": str(case / value) for key, value in {
        "WORLD": "world", "SETTINGS": "settings.json", "LIGHTING": "lighting.json", "INVENTORY": "inventory.json",
        "PROFILE": "world-profile.csv", "GPU_PROFILE": "gpu-profile.csv", "STREAM_PROFILE": "stream-profile.csv"}.items()}
    overrides.update(OCTARYN_CLIENT_GRAPHICS_API=args.backend, OCTARYN_CLIENT_UPSCALER=args.upscaler,
                     OCTARYN_CLIENT_WORLD_BATCH="required", OCTARYN_CLIENT_FRAMES_IN_FLIGHT="2")
    environment.update(overrides)
    command = [str(executable), "--benchmark-settings", "--benchmark-hidden", "--benchmark-seconds", str(args.seconds),
               "--benchmark-streaming-speed", str(args.speed), "--render-distance", str(args.radius)]
    report = {"status": "running", "backend": args.backend, "upscaler": args.upscaler, "seconds": args.seconds,
              "speed_mps": args.speed, "radius": args.radius, "width": args.width, "height": args.height,
              "fixture": "linear camera and stream center; authoritative player stays stationary",
              "no_lod": True, "warmup_seconds": 5, "validation_enabled": False, "command": command,
              "settings": settings, "lighting": lighting, "environment_overrides": overrides,
              "client_sha256": digest(executable), "bundle_content": content_digest(bundle), "evidence": str(case)}
    path = case / "result.json"
    path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"streaming_benchmark_started evidence={case}", flush=True)
    begin = time.monotonic()
    try:
        with (case / "client.log").open("wb") as output:
            process = subprocess.Popen(command, cwd=case, env=environment, stdout=output, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=300)
            except subprocess.TimeoutExpired:
                stop_owned(process, case)
                raise RuntimeError("Streaming benchmark exceeded 300 seconds; partial evidence retained")
        report.update(inspect_run(case, code, args))
        require(digest(executable) == report["client_sha256"], "Packaged executable changed during measurement")
        require(content_digest(bundle) == report["bundle_content"], "Packaged content changed during measurement")
        report["status"] = "passed"
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        report["wall_seconds"] = time.monotonic()-begin
        path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"streaming_benchmark=passed evidence={case}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(f"streaming_benchmark=failed: {error}", file=sys.stderr)
        sys.exit(1)
