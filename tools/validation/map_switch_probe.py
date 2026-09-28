"""Exercise twenty A-B-A renderer and authoritative-session replacement cycles."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import statistics
import hashlib

from capture_ui_effects import isolated_environment
from make_tile_fixture import payload
from case_evidence import record_build

ROOT = Path(__file__).resolve().parents[2]


def memory_tail(samples):
    # Ignore ten warmups. Bound range and recurring linear growth separately:
    # driver/managed caches may add noise, but per-switch ownership must plateau.
    tail = samples[10:]
    result = {}
    for key in ("resident", "gpu", "gpu_os"):
        if any(key not in row for row in tail):
            raise RuntimeError("Missing memory telemetry: " + key)
        values = [row[key] for row in tail]
        xmean = (len(values) - 1) / 2
        ymean = statistics.mean(values)
        slope = sum((i-xmean)*(value-ymean) for i,value in enumerate(values)) / sum(
            (i-xmean)**2 for i in range(len(values)))
        spread = max(values) - min(values)
        result[key] = {"minimum": min(values), "maximum": max(values), "median": statistics.median(values),
                       "range": spread, "bytes_per_session": slope,
                       "passed": spread <= 64*1024**2 and slope <= 1024**2}
    return result


def make_world(path, center, color):
    path.mkdir()
    (path / "main.glb").write_bytes(payload(center, color))
    (path / "map.json").write_text(json.dumps({"version": 1, "map": "main.glb",
        "spawn": [center, 3, -6], "yaw": 0.0, "pitch": -0.2}), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("dx12", "vulkan"), default="dx12")
    parser.add_argument("--timeout", type=int, default=360)
    args = parser.parse_args()
    evidence = Path(tempfile.mkdtemp(prefix="map-switch-", dir=ROOT / "logs/client"))
    worlds = [evidence / "a", evidence / "b"]
    for world, center, color in zip(worlds, (0, 40), ([.8,.1,.1,1], [.1,.2,.8,1])):
        make_world(world, center, color)
    env = isolated_environment(evidence, args.backend, 2560, 1440)
    record_build(ROOT / "build/release-windows/client/bundle", evidence)
    (evidence / "asset-build.json").write_text(json.dumps({str(path.relative_to(evidence)):
        hashlib.sha256(path.read_bytes()).hexdigest() for world in worlds
        for path in (world / "main.glb", world / "map.json")}, indent=2))
    command = [str(ROOT / "build/release-windows/client/bundle/Octaryn.Client.exe"),
               "--validate-map-switches", *map(str, worlds), "--benchmark-hidden", "--benchmark-settings"]
    (evidence / "command.json").write_text(json.dumps(command, indent=2))
    with (evidence / "client.log").open("wb") as log:
        process = subprocess.Popen(command, cwd=evidence, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            for world in worlds:
                (world / "runtime").mkdir(exist_ok=True)
                (world / "runtime/shutdown.request").write_text("stop\n")
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=10)
            raise RuntimeError("Map switch watchdog expired: " + str(evidence))
    text = (evidence / "client.log").read_text(errors="replace")
    if code or "map_switch=passed cycles=20 sessions=41 switches=40" not in text:
        raise RuntimeError(f"Map switch failed exit={code}: {evidence}")
    ready = re.findall(r"authoritative_player_ready eye=([\d.-]+),([\d.-]+),([\d.-]+)", text)
    if len(ready) != 41 or any(abs(float(p[0]) - (0 if i % 2 == 0 else 40)) > .1 for i,p in enumerate(ready)):
        raise RuntimeError("Authority map identity did not alternate: " + str(evidence))
    samples = [dict((key, int(value)) for key,value in re.findall(r"(\w+)=(\d+)", line))
               for line in text.splitlines() if line.startswith("map_switch_sample ")]
    if len(samples) != 41 or len(set(row["geometry"] for row in samples)) != 1:
        raise RuntimeError("Map geometry ownership did not stabilize: " + str(evidence))
    if any(marker in text for marker in ("Validation Error", "VUID-", "D3D12 ERROR", "Map startup timed out")):
        raise RuntimeError("Graphics or readiness validation failed: " + str(evidence))
    memory = memory_tail(samples)
    result = {"status": "passed" if all(row["passed"] for row in memory.values()) else "failed",
              "backend": args.backend, "cycles": 20, "samples": samples, "memory_tail": memory,
              "limits": {"warmups": 10, "maximum_range_bytes": 64*1024**2, "maximum_bytes_per_session": 1024**2}}
    (evidence / "result.json").write_text(json.dumps(result, indent=2))
    if result["status"] != "passed":
        raise RuntimeError("Map switch tail memory did not plateau: " + str(evidence))
    print("map_switch_probe=passed evidence=" + str(evidence))


if __name__ == "__main__":
    main()
