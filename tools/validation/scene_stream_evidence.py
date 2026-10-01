"""Reject gaps, premature publication and unfenced scene-owner retirement."""
import csv
import json
import math
from pathlib import Path
import re

from gameplay_route_report import summarize_gameplay_route
from reflection_fixture import inspect as inspect_reflection
from scene_stream_fixture import DISTANCE


BUDGET = 512 * 1024**2
CONTINUITY = {"frame", "generation", "resident", "pending", "cpu_pending", "pending_roots", "pending_rays",
              "ray_requested", "ray_enabled", "coverage_complete", "reflection_valid", "ray_generation",
              "reservation_bytes", "allocated_reservation_bytes", "retired_bytes", "budget", "retired", "fence_signal", "fence_completed"}


def records(log, marker, required):
    rows = []
    for line in log.splitlines():
        if not line.startswith(marker + " "):
            continue
        row = {}
        for token in line.split()[1:]:
            match = re.fullmatch(r"(\w+)=(\d+)", token)
            if not match or match[1] in row:
                raise RuntimeError("Malformed " + marker + " record")
            row[match[1]] = int(match[2])
        if not required.issubset(row):
            raise RuntimeError("Incomplete " + marker + " record")
        rows.append(row)
    if not rows:
        raise RuntimeError("Missing " + marker + " evidence")
    return rows


def continuity(log, first, last):
    all_rows = records(log, "scene_continuity", CONTINUITY)
    rows = [row for row in all_rows if first <= row["frame"] <= last]
    if [row["frame"] for row in rows] != list(range(first, last + 1)):
        raise RuntimeError("Missing, duplicate or reordered scene continuity frames")
    for row in rows:
        if row["budget"] != BUDGET or row["allocated_reservation_bytes"] + row["retired_bytes"] > BUDGET:
            raise RuntimeError("Scene active/pending/retired reservation exceeded the unchanged budget")
        if row["pending_roots"] > row["pending"] or row["pending_rays"] > row["pending"]:
            raise RuntimeError("Invalid staged readiness counts")
        if not all(row[field] == 1 for field in ("ray_requested", "ray_enabled", "coverage_complete", "reflection_valid")):
            raise RuntimeError("Ready reflections disappeared during incoming scene publication")
        if not row["ray_generation"] or not row["resident"]:
            raise RuntimeError("Continuity frame lacks a published ray scene")
    pending = [row for row in rows if row["pending"]]
    if not pending or not any(row["pending_roots"] < row["pending"] for row in pending) or not any(
            row["pending_rays"] < row["pending"] for row in pending):
        raise RuntimeError("No incoming raster-root and ray preparation interval was observed")
    publications = records(log, "scene_part_published",
                           {"frame", "part", "instances", "roots_ready", "rays_ready", "generation"})
    incoming = [row for row in publications if first < row["frame"] <= last]
    if not incoming or any(row["roots_ready"] != 1 or row["rays_ready"] != 1 or not row["instances"]
                           for row in publications):
        raise RuntimeError("Incoming scene parts published before raster roots and rays were ready")
    if not {4, 5, 6, 7}.issubset({row["part"] for row in incoming}):
        raise RuntimeError("The four authored incoming mirror parts were not published")
    retired = records(log, "scene_part_retired", {"frame", "part", "fence_signal"})
    collected = records(log, "scene_part_collected",
                        {"frame", "part", "fence_signal", "fence_completed", "ray_idle", "upload_idle"})
    retired = [row for row in retired if first < row["frame"] <= last]
    collected = [row for row in collected if first < row["frame"] <= last]
    by_part = {row["part"]: row for row in retired}
    if not {0, 1, 2, 3}.issubset(by_part) or len(by_part) != len(retired):
        raise RuntimeError("The old station did not retire exactly once")
    completed = set()
    for row in collected:
        prior = by_part.get(row["part"])
        if (not prior or row["part"] in completed or row["frame"] < prior["frame"] or
                row["fence_signal"] != prior["fence_signal"] or row["fence_completed"] < row["fence_signal"] or
                row["ray_idle"] != 1 or row["upload_idle"] != 1):
            raise RuntimeError("Retired scene owner released before its actual consumer fences")
        completed.add(row["part"])
    if not {0, 1, 2, 3}.issubset(completed) or rows[-1]["retired"] or rows[-1]["pending"]:
        raise RuntimeError("Scene publication or retirement failed to settle")
    return dict(first_frame=first, last_frame=last, checked_frames=len(rows), pending_frames=len(pending),
                incoming_parts=sorted({row["part"] for row in incoming}), retired_parts=sorted(completed),
                maximum_reservation_bytes=max(row["allocated_reservation_bytes"] + row["retired_bytes"] for row in rows),
                maximum_desired_plan_bytes=max(row["reservation_bytes"] for row in rows),
                budget_bytes=BUDGET, continuous_reflections=True, fenced_retirement=True)


def inspect(case):
    case = Path(case)
    result = json.loads((case / "result.json").read_text())
    if result.get("status") != "captured" or result.get("exit_code") != 0 or result.get("watchdog_max_frame_ms") != 50:
        raise RuntimeError("Capture did not pass with the original 50ms watchdog")
    if result.get("uncapped_fps") or not result.get("virtual_geometry", {}).get("scene_continuity"):
        raise RuntimeError("Missing capped scene continuity qualification")
    route = summarize_gameplay_route(case, result["gameplay_route"])
    if route["authority_distance_metres"] < DISTANCE - 1:
        raise RuntimeError("Authority did not reach the incoming station")
    log = (case / "client.log").read_text(encoding="utf-8", errors="replace")
    observations = re.findall(r"^world_capture frame=(\d+) nonclear_pixels=\d+ eye=([\d.eE+,-]+) "
                              r"yaw=([\d.eE+-]+) pitch=([\d.eE+-]+) fov=[\d.eE+-]+ path=", log, re.M)
    cameras = {int(frame): tuple(map(float, eye.split(","))) + (float(yaw), float(pitch))
               for frame, eye, yaw, pitch in observations}
    selected = {}
    for capture in result["captures"]:
        frame = int(capture["lighting"]["render_frame"])
        camera = cameras.get(frame)
        if not camera or abs(camera[2] - 6) > .5 or abs(camera[3]) > .1 or abs(camera[4] + .85) > .1:
            continue
        for name, center in (("before", 0), ("after", DISTANCE)):
            if abs(camera[0] - center) < 1:
                selected.setdefault(name, []).append((frame, case / capture["path"], center))
    if set(selected) != {"before", "after"}:
        raise RuntimeError("Missing settled GPU captures before and after actual authority travel")
    before, after = selected["before"][0], selected["after"][-1]
    evidence = continuity(log, before[0], after[0])
    states = {row["frame"]: row for row in records(log, "world_capture_scene",
        {"frame", "resident", "requested", "total", "generation", "all_manifest_ready"})}
    for frame, _, _ in (before, after):
        state = states.get(frame)
        if (not state or state["total"] != 9 or state["resident"] != state["requested"] or
                not 0 < state["resident"] < state["total"] or state["all_manifest_ready"] != 0):
            raise RuntimeError("Capture must certify its requested region without claiming the distant station ready")
    with (case / "lighting.csv").open(newline="") as stream:
        timing = {int(row["frame"]): row for row in csv.DictReader(stream)}
    for frame in range(before[0], after[0] + 1):
        row = timing.get(frame)
        if row is None or sum(float(row.get(key, 0)) for key in
                              ("reflection_trace_ms", "reflection_classify_ms")) <= 0:
            raise RuntimeError("An actual reflection GPU pass was missing inside the continuity interval")
    materials = {name: inspect_reflection(item[1], True, item[2])
                 for name, item in (("before", before), ("after", after))}
    for name, item in (("before", before), ("after", after)):
        camera = materials[name]["offscreen_evidence"]["camera"]
        # Reflection in the y=0 plane has the same distance as a direct ray to
        # the panel mirrored below it. This bounds every authored colored hit.
        distance = max(math.dist(camera[:3], [item[2]+x, -y, 0])
                       for x in (-2.2, 2.2) for y in (2.6, 4.2))
        if distance >= 15:
            raise RuntimeError("Authored reflection escaped the fixture's local hit-distance bound")
        materials[name]["maximum_authored_reflection_distance"] = distance
        materials[name]["global_manifest_ready"] = False
    evidence.update(authority=route, materials=materials, reflection_distance=64,
        coverage_scope="Requested 128m region and active ray snapshot; distant station explicitly not globally ready",
        scope="Small prepared scene mechanism; no Zorah-scale, FPS or platform-wide acceptance")
    return evidence
