"""Attribute authority wall tails without converting hardware cycles to time."""
import argparse
import csv
import json
import math
from pathlib import Path

from authority_item_listener import stats


def read_rows(path):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    if not rows or any(row.get("schema") != "2" for row in rows):
        raise ValueError("Execution attribution requires complete schema 2 profiles")
    for row in rows:
        for key, value in row.items():
            if not math.isfinite(float(value)):
                raise ValueError("Nonfinite profile value: " + key)
        if row["thread_cycles_valid"] not in ("0", "1"):
            raise ValueError("Invalid cycle validity marker")
        if float(row["gc_pause_ms"]) < 0 or int(row["thread_cycles"]) < 0:
            raise ValueError("Negative execution counter")
    return rows


def sample(row, wall_key, identity):
    return {identity: int(row[identity]), "wall_ms": float(row[wall_key]),
            "thread_cycles": int(row["thread_cycles"]) if row["thread_cycles_valid"] == "1" else None,
            "process_gc_pause_ms": float(row["gc_pause_ms"]),
            "process_alloc_bytes": int(row["process_alloc_bytes"]),
            "phase_wall_ms": {key: float(value) for key, value in row.items()
                              if key != wall_key and (key.endswith("_wall_ms") or key.endswith("_schedule_ms"))
                              and key != "elapsed_wall_ms"}}


def distribution(rows, wall_key, identity):
    worst = sorted(rows, key=lambda row: float(row[wall_key]), reverse=True)[:10]
    valid = [int(row["thread_cycles"]) for row in rows if row["thread_cycles_valid"] == "1"]
    return dict(wall_ms=stats([float(row[wall_key]) for row in rows]),
                thread_cycles=stats(valid) if valid else None,
                valid_cycle_samples=len(valid),
                process_gc_pause_ms=stats([float(row["gc_pause_ms"]) for row in rows]),
                worst=[sample(row, wall_key, identity) for row in worst])


def summarize(case, warmup=300):
    loops = read_rows(case / "loop.csv")
    modules = read_rows(case / "module.csv")
    active = []
    intervals = []
    origin = int(loops[0]["authority_tick"])
    last_active = None
    window_work = float(loops[0]["total_wall_ms"])
    window_pumps = 1
    leading_partial = None
    for previous, row in zip(loops, loops[1:]):
        window_work += float(row["total_wall_ms"])
        window_pumps += 1
        if int(row["pump"]) != int(previous["pump"]) + 1:
            raise ValueError("Missing listener pump record")
        if float(row["elapsed_wall_ms"]) < float(previous["elapsed_wall_ms"]):
            raise ValueError("Service clock regressed")
        delta = int(row["authority_tick"]) - int(previous["authority_tick"])
        if delta < 0:
            raise ValueError("Authority clock regressed")
        if delta == 0:
            continue
        if int(row["authority_tick"]) >= origin + warmup:
            active.append(row)
            if last_active is not None:
                intervals.append(dict(pump=int(row["pump"]), authority_tick=int(row["authority_tick"]),
                    tick_delta=int(row["authority_tick"]) - int(last_active["authority_tick"]),
                    completion_gap_ms=float(row["elapsed_wall_ms"]) - float(last_active["elapsed_wall_ms"]),
                    profiled_owner_work_ms=window_work, pump_count=window_pumps))
        if last_active is None:
            leading_partial = dict(pump_count=window_pumps, profiled_owner_work_ms=window_work)
        last_active = row
        window_work = 0.0
        window_pumps = 0
    for previous, row in zip(modules, modules[1:]):
        if int(row["frame"]) != int(previous["frame"]) + 1:
            raise ValueError("Missing module tick record")
    modules = modules[warmup:]
    if not active or not modules or not intervals:
        raise ValueError("Insufficient post-warmup execution records")
    authority = {}
    with (case / "authority.csv").open() as stream:
        for row in csv.DictReader(stream):
            frame = int(row["frame"])
            if frame in authority:
                raise ValueError("Duplicate authority frame")
            authority[frame] = row
    module_report = distribution(modules, "whole_tick_ms", "frame")
    for row in module_report["worst"]:
        frame = row["frame"]
        if frame not in authority:
            raise ValueError("Missing authority phase match")
        row["authority_phases"] = {key: float(value) for key, value in authority[frame].items() if key != "frame"}
    return dict(schema=1, case=str(case), warmup_authority_ticks=warmup,
        advancing_pumps=distribution(active, "total_wall_ms", "pump"), module_ticks=module_report,
        authority_service=dict(completion_gap_ms=stats([row["completion_gap_ms"] for row in intervals]),
            profiled_owner_work_per_completion_ms=stats([row["profiled_owner_work_ms"] for row in intervals]),
            gaps_above_60hz=sum(row["completion_gap_ms"] > 1000 / 60 for row in intervals),
            gaps_above_two_ticks=sum(row["completion_gap_ms"] > 2000 / 60 for row in intervals),
            pumps_with_catchup=sum(row["tick_delta"] > 1 for row in intervals),
            worst=sorted(intervals, key=lambda row: row["completion_gap_ms"], reverse=True)[:10],
            worst_owner_work=sorted(intervals, key=lambda row: row["profiled_owner_work_ms"], reverse=True)[:10],
            leading_partial_excluded=leading_partial,
            trailing_unclosed_excluded=dict(pump_count=window_pumps, profiled_owner_work_ms=window_work),
            owner_work_scope="Sum of every profiled pump after the previous authority completion through this completion, including zero-tick network and replication polls. Multi-tick catch-up windows are not divided by tick count. Native-loop sleep, work on other threads, and profile serialization after the End timestamp are outside this sum. Completion gaps include intervening sleep/instrumentation. Incomplete leading/trailing windows are reported separately, never treated as complete ticks."),
        interpretation="Cycles measure executing owner-thread work; never elapsed time. GC pause is process-wide and may overlap the interval. Low cycles with high wall time suggest waiting/descheduling but do not prove a particular scheduler cause. Completion gaps include idle polling, sleep and catch-up; slight 60Hz overruns alone are not missed whole ticks.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", type=Path)
    args = parser.parse_args()
    report = summarize(args.case)
    (args.case / "execution-summary.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
