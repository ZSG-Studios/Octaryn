"""Compare two completed streaming runs with matching route, settings and geometry."""
import argparse
import json
from pathlib import Path
import sys

from benchmark_presentation import numeric, read_csv, require


def ratio(before, after):
    return {"before": before, "after": after, "change_percent": 100*(after/before-1) if before else None}


def load_profile(report, source):
    rows = read_csv(source.parent / "stream-profile.csv")
    startup = [row for row in rows if row["phase"] == "startup"]
    resident = next((row for row in startup if int(row["columns"]) == report["final_columns"]
                     and int(row["pending_meshes"]) == 0), None)
    require(resident, "Startup never reached complete geometry")
    moving = [row for row in rows if row["phase"] == "moving"]
    settled = [row for row in rows if row["phase"] == "settle"]
    require(moving and settled, "Missing motion or settle samples")
    loading_seconds = sum(numeric(row, "frame_ms")/1000 for row in moving if int(row["loading"]))
    population = numeric(resident, "time_seconds")-numeric(startup[0], "time_seconds")
    return {"initial_population_seconds": population,
            "initial_columns_per_second": report["final_columns"]/population if population else 0,
            "motion_loading_seconds": loading_seconds,
            "final_settle_seconds": numeric(settled[-1], "time_seconds")-numeric(settled[0], "time_seconds"),
            "minimum_resident_fraction": report["columns_min"]/report["final_columns"]}


def compare(before_path, after_path):
    before = json.loads(before_path.read_text(encoding="utf-8"))
    after = json.loads(after_path.read_text(encoding="utf-8"))
    require(before["status"] == after["status"] == "passed", "Both runs must have passed qualification")
    fields = ("backend", "upscaler", "seconds", "speed_mps", "radius", "width", "height", "fixture",
              "no_lod", "warmup_seconds", "validation_enabled", "settings", "lighting", "api", "device",
              "world_generation", "final_center", "final_camera", "final_columns", "final_quads",
              "final_snapshot_identities")
    mismatches = [key for key in fields if before[key] != after[key]]
    require(not mismatches, "Cannot compare mismatched run contracts: " + ", ".join(mismatches))
    populations = [load_profile(report, path) for report, path in ((before, before_path), (after, after_path))]
    groups = {}
    for phase in ("moving", "moving_loading", "moving_resident"):
        left, right = before[phase], after[phase]
        group = {"samples": ratio(left["samples"], right["samples"])}
        if left["samples"] and right["samples"]:
            group["frame_ms"] = {key: ratio(left["phase_timings_ms"]["frame_ms"][key],
                                           right["phase_timings_ms"]["frame_ms"][key])
                                 for key in ("mean", "p50", "p95", "p99", "max")}
            group["frames_over_16_67_ms"] = ratio(left["frames_over_16_67_ms"], right["frames_over_16_67_ms"])
            group["frames_over_33_33_ms"] = ratio(left["frames_over_33_33_ms"], right["frames_over_33_33_ms"])
            group["frames_over_50_ms"] = ratio(left["frames_over_50_ms"], right["frames_over_50_ms"])
        groups[phase] = group
    return {"status": "passed", "before": str(before_path.resolve()), "after": str(after_path.resolve()),
            "matching_contract_fields": list(fields), "matching_final_quads": before["final_quads"],
            "before_client_sha256": before["client_sha256"], "after_client_sha256": after["client_sha256"],
            "frame_comparison": groups,
            "loading_comparison": {key: ratio(populations[0][key], populations[1][key]) for key in populations[0]},
            "memory_comparison": {key: ratio(before[key], after[key]) for key in ("gpu_bytes_max", "final_gpu_bytes")},
            "limits": "Timed camera/stream route; excludes authoritative player travel. Population begins at first player-ready frame. Single runs are not statistical confidence intervals."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = compare(args.before, args.after)
    encoded = json.dumps(result, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(f"streaming_comparison=failed: {error}", file=sys.stderr)
        sys.exit(1)
