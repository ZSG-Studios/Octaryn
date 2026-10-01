"""Measure one complete primitive's spatial ordering in an isolated catalog copy."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

from capture_watchdog import ProcessTree, resume_owned_process
from cook_map_textures_guarded import MemoryStatus, ProcessMemory, JobLimits, GIB

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "build/release-windows/tools"


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(8 << 20):
            value.update(block)
    return value.hexdigest()


def guarded(command, log, scratch, timeout):
    api = ctypes.WinDLL("kernel32", use_last_error=True)
    api.GlobalMemoryStatusEx.argtypes = [ctypes.POINTER(MemoryStatus)]
    api.GlobalMemoryStatusEx.restype = wintypes.BOOL
    api.K32GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessMemory), wintypes.DWORD]
    api.K32GetProcessMemoryInfo.restype = wintypes.BOOL

    def available():
        status = MemoryStatus(); status.length = ctypes.sizeof(status)
        if not api.GlobalMemoryStatusEx(ctypes.byref(status)):
            raise ctypes.WinError(ctypes.get_last_error())
        return status.avail_phys

    result = dict(command=[str(item) for item in command], status="not_started", peak_rss_bytes=0,
                  peak_private_bytes=0, peak_scratch_bytes=0, minimum_available_bytes=available(),
                  timeout_seconds=timeout, minimum_free_bytes=4 * GIB, private_limit_bytes=GIB)
    started = time.monotonic(); process = tree = None
    try:
        if result["minimum_available_bytes"] < 4 * GIB:
            raise RuntimeError("less than 4GiB available before launch")
        with log.open("w", encoding="utf-8") as output:
            process = subprocess.Popen(result["command"], cwd=ROOT, stdout=output, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS | 4)
            tree = ProcessTree(process)
            limits = JobLimits(); limits.basic.flags = 0x2100; limits.process_memory = GIB
            if not tree.api.SetInformationJobObject(tree.job, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
                raise ctypes.WinError(ctypes.get_last_error())
            resume_owned_process(process)
            while process.poll() is None:
                memory = ProcessMemory(); memory.length = ctypes.sizeof(memory)
                if not api.K32GetProcessMemoryInfo(wintypes.HANDLE(int(process._handle)), ctypes.byref(memory), ctypes.sizeof(memory)):
                    if process.poll() is not None:
                        break
                    raise ctypes.WinError(ctypes.get_last_error())
                result["peak_rss_bytes"] = max(result["peak_rss_bytes"], memory.peak_rss)
                result["peak_private_bytes"] = max(result["peak_private_bytes"], memory.private, memory.peak_pagefile)
                result["minimum_available_bytes"] = min(result["minimum_available_bytes"], available())
                current = 0
                for item in scratch.rglob("*"):
                    if item.suffix not in {".run", ".view"}:
                        continue
                    try:
                        current += item.stat().st_size
                    except FileNotFoundError:
                        pass
                result["peak_scratch_bytes"] = max(result["peak_scratch_bytes"], current)
                if result["minimum_available_bytes"] < 4 * GIB or memory.private > GIB:
                    raise RuntimeError("memory guard exceeded")
                if time.monotonic() - started > timeout:
                    raise RuntimeError("spatial ordering deadline exceeded")
                time.sleep(.1)
            result["exit_code"] = process.wait(timeout=5)
            result["status"] = "completed" if process.returncode == 0 else "failed"
    except Exception as error:
        result["status"] = "stopped"; result["error"] = str(error)
    finally:
        if tree:
            tree.close()
        elif process and process.poll() is None:
            process.kill(); process.wait(timeout=5)
        result["seconds"] = round(time.monotonic() - started, 3)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", required=True, type=Path)
    parser.add_argument("--canonical", required=True, type=Path)
    parser.add_argument("--primitive", required=True, type=int)
    parser.add_argument("--eye", required=True, nargs=3, type=float)
    parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args()
    if os.name != "nt" or not 0 < args.timeout <= 300:
        parser.error("Windows only; deadline must be positive and at most300seconds")
    catalog = args.catalog.resolve(); canonical = args.canonical.resolve(); root = catalog.parent
    if catalog == canonical:
        parser.error("measurement requires a separate catalog copy")
    before = json.loads(catalog.read_text()); primitive = before["primitives"][args.primitive]
    nodes = [item for item in before["instances"] if item["mesh"] == primitive["mesh"]]
    if not nodes:
        parser.error("selected primitive has no original scene instances")
    bounds = nodes[0]["bounds"]
    instance_eye = [(bounds[axis] + bounds[axis + 3]) * .5 for axis in range(3)]
    report = {"catalog": str(catalog), "canonical": str(canonical), "primitive": args.primitive,
              "mesh": primitive["mesh"], "source_primitive": primitive["primitive"], "triangles": primitive["triangles"],
              "authored_eye": args.eye, "affected_instance_eye": instance_eye, "affected_node": nodes[0]["node"]}
    canonical_hash = digest(canonical)
    report["source_sha256_before"] = {item["path"]: digest(Path(item["path"])) for item in before["resources"]}
    cook = TOOLS / "map-scene-cook/octaryn_map_scene_cook.exe"
    probe = TOOLS / "scene-residency-probe/octaryn_scene_catalog_probe.exe"

    def plans(label, eye):
        lines = []
        for radius in (3, 8, 24):
            output = subprocess.run([str(probe), str(catalog), *map(str, eye), str(radius), str(radius)],
                                    cwd=ROOT, capture_output=True, text=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
            if output.returncode:
                raise RuntimeError(output.stderr)
            lines.append(output.stdout.strip())
        (root / f"plans-{label}.log").write_text("\n".join(lines) + "\n")
        return lines

    report["before"] = plans("before", args.eye)
    report["affected_instance_before"] = plans("affected-before", instance_eye)
    report["ordering"] = guarded([cook, "--order", catalog, args.primitive, 1], root / "order.log", root, args.timeout)
    if report["ordering"]["status"] == "completed":
        # Verification loads and checks the entire permutation; exact bounds are already prepared.
        report["verification"] = guarded([cook, "--bounds", catalog, primitive["first_part"], primitive["part_count"]],
                                         root / "verify.log", root, args.timeout)
        report["after"] = plans("after", args.eye)
        report["affected_instance_after"] = plans("affected-after", instance_eye)
        after = json.loads(catalog.read_text()); ordered = after["primitives"][args.primitive]
        report["permutation_bytes"] = (root / ordered["triangle_order"]).stat().st_size
        report["permutation_hash"] = ordered["triangle_order_hash"]
        report["nodes_unchanged"] = before["instances"] == after["instances"]
        report["source_identity_unchanged"] = before["source_hash"] == after["source_hash"]
        report["complete_triangle_count"] = sum(item["triangle_count"] for item in after["parts"] if item["primitive"] == args.primitive)
    report["source_sha256_after"] = {item["path"]: digest(Path(item["path"])) for item in before["resources"]}
    report["source_unchanged"] = report["source_sha256_before"] == report["source_sha256_after"]
    report["canonical_unchanged"] = canonical_hash == digest(canonical)
    report["passed"] = report["ordering"]["status"] == "completed" and report.get("verification", {}).get("status") == "completed" and \
        report["source_unchanged"] and report["canonical_unchanged"] and report.get("nodes_unchanged") and \
        report.get("complete_triangle_count") == primitive["triangles"]
    (root / "measurement.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
