"""Compare three matched listener runs per item workload without hiding idle polls."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics


def load(cases):
    groups = {}
    for case in cases:
        report = json.loads((case / "result.json").read_text())
        if report.get("status") != "passed" or report.get("timing_qualified") is not True:
            raise ValueError("Run is not timing qualified: " + str(case))
        key = (report["count"], report["awake"])
        identity = hashlib.sha256((case / "world/main.glb").read_bytes()).hexdigest()
        entry = dict(case=str(case), map_sha256=identity, warmup=report["warmup_authority_ticks"],
                     loop=report["advancing_authority_pumps_ms"], module=report["module_ticks"]["whole_tick_ms"])
        groups.setdefault(key, []).append(entry)
    if any(len(runs) != 3 for runs in groups.values()):
        raise ValueError("Require exactly three runs per count/awake workload")
    return groups


def metrics(runs):
    result = {}
    for owner in ("loop", "module"):
        result[owner] = {key: dict(per_run=[run[owner][key] for run in runs],
                                  median_of_runs=statistics.median(run[owner][key] for run in runs),
                                  minimum=min(run[owner][key] for run in runs),
                                  maximum=max(run[owner][key] for run in runs))
                         for key in ("median", "p95", "p99", "worst")}
    return result


def compare(before_cases, after_cases):
    before, after = load(before_cases), load(after_cases)
    if before.keys() != after.keys():
        raise ValueError("Before/after workload groups differ")
    workloads = []
    for key in sorted(before):
        runs = before[key] + after[key]
        if len({(run["map_sha256"], run["warmup"]) for run in runs}) != 1:
            raise ValueError("Map or warmup differs between matched workloads")
        previous, current = metrics(before[key]), metrics(after[key])
        budget = 6 if key == (1000, 256) else 14 if key == (10000, 10000) else None
        ratio = current["loop"]["p99"]["median_of_runs"] / previous["loop"]["p99"]["median_of_runs"]
        workloads.append(dict(count=key[0], awake=key[1], before=previous, after=current,
            p99_median_reduction_percent=(1 - ratio) * 100,
            p99_entire_after_range_below_before_range=current["loop"]["p99"]["maximum"] < previous["loop"]["p99"]["minimum"],
            p99_budget_ms=budget, all_three_p99_within_budget=budget is not None and current["loop"]["p99"]["maximum"] <= budget,
            before_cases=[run["case"] for run in before[key]], after_cases=[run["case"] for run in after[key]]))
    return dict(schema=1, workloads=workloads,
        scope="Sustained airborne + sleeping, actual listener/module/Box3D and managed reflection receiver; no graphics/contact-heavy/Bistro qualification",
        timing="Wall time; OS descheduling and GC can contribute. Counter evidence alone cannot separate CPU execution from pauses.",
        statistic="Per-run distributions and median of three run statistics; idle listener polls excluded from authority-pump comparison")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", nargs="+", type=Path, required=True)
    parser.add_argument("--after", nargs="+", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = compare(args.before, args.after)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
