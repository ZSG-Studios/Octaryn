"""Validate actual authority traversal; presentation-camera motion cannot satisfy this report."""
import argparse
import csv
import json
import math
from pathlib import Path
import re


def summarize_gameplay_route(case, route):
    case = Path(case)
    with (case / "gameplay-route.csv").open() as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) < 2:
        raise RuntimeError("Missing gameplay authority evidence")
    phases = route["phases"]
    groups = [[] for _ in phases]
    distance = 0.
    previous = None
    authority_samples = 0
    for row in rows:
        for key, value in row.items():
            if key != "phase" and not math.isfinite(float(value)):
                raise RuntimeError("Nonfinite gameplay route evidence")
        index = int(row["phase_index"])
        if not 0 <= index < len(phases) or row["phase"] != phases[index]["name"]:
            raise RuntimeError("Route phase does not match authored input")
        groups[index].append(row)
        if int(row["ack"]) > int(row["sent_input"]):
            raise RuntimeError("Authority acknowledged an input beyond the client's sent sequence")
        if previous is not None:
            for key in ("seconds", "authority_tick", "ack", "sent_input", "phase_index", "authority_distance"):
                if float(row[key]) < float(previous[key]):
                    raise RuntimeError("Gameplay evidence regressed: " + key)
            delta = math.hypot(float(row["authority_x"]) - float(previous["authority_x"]),
                               float(row["authority_z"]) - float(previous["authority_z"]))
            if row["authority_tick"] == previous["authority_tick"]:
                if delta > .0001:
                    raise RuntimeError("Authority pose changed without an authority tick")
            else:
                distance += delta
                authority_samples += 1
        if abs(float(row["authority_distance"]) - distance) > .01 + distance * 1e-5:
            raise RuntimeError("Reported traversal distance differs from received authority positions")
        previous = row
    log = (case / "client.log").read_text(errors="replace")
    outcomes = re.findall(r"gameplay_route_phase name=(\S+) completed=1 passed=(\d)", log)
    if outcomes != [(phase["name"], "1") for phase in phases] or not re.search(
            r"gameplay_route_result passed=1 complete=1 ", log):
        raise RuntimeError("Route incomplete, blocked, or failed an authored acceptance condition")
    if "profile_writer_failed" in log:
        raise RuntimeError("Gameplay evidence was dropped by the bounded profile writer")
    phase_reports = []
    for phase, values in zip(phases, groups):
        if not values:
            raise RuntimeError("An authored route phase produced no authority records")
        travelled = max(float(row["phase_distance"]) for row in values)
        target = phase.get("target")
        error = math.hypot(float(values[-1]["authority_x"]) - target[0],
                           float(values[-1]["authority_z"]) - target[1]) if target else None
        if travelled + .01 < phase.get("min_distance", 0) or (
                target and error > phase.get("tolerance", .5) + .001):
            raise RuntimeError("Authority CSV did not satisfy authored distance/waypoint")
        phase_reports.append(dict(name=phase["name"], authority_distance_metres=travelled,
                                  target_error_metres=error, blocked_rows=sum(int(row["blocked"]) for row in values)))
    elapsed = float(rows[-1]["seconds"]) - float(rows[0]["seconds"])
    ack_progress = int(rows[-1]["ack"]) - int(rows[0]["ack"])
    moving = any(phase.get("forward", 0) or phase.get("strafe", 0) for phase in phases)
    traversal = distance >= .1 and authority_samples > 1 and ack_progress > 0
    if moving and not traversal:
        raise RuntimeError("Stationary authority cannot qualify a gameplay traversal workload")
    return dict(schema=1, route_complete=True, traversal_observed=traversal,
                authority_distance_metres=distance, elapsed_seconds=elapsed,
                average_authority_speed_mps=distance / elapsed if elapsed > 0 else 0,
                authority_samples=authority_samples, acknowledged_input_progress=ack_progress,
                maximum_pending=max(int(row["pending"]) for row in rows), phases=phase_reports,
                distance_scope="XZ polyline of received authority snapshots; may undercount motion between snapshots")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", type=Path)
    args = parser.parse_args()
    route = json.loads((args.case / "result.json").read_text())["gameplay_route"]
    report = summarize_gameplay_route(args.case, route)
    (args.case / "gameplay-route-summary.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
