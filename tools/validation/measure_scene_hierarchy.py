"""Guard and measure a source-faithful hierarchy window or whole primitive cook."""
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
COOK = ROOT / "build/release-windows/tools/map-scene-cook/octaryn_map_scene_cook.exe"


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(8 << 20):
            value.update(block)
    return value.hexdigest()


def file_bytes(root, suffixes=None):
    total = 0
    for path in root.rglob("*"):
        try:
            if path.is_file() and (suffixes is None or path.suffix in suffixes):
                total += path.stat().st_size
        except FileNotFoundError:
            pass
    return total


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
                  priority="normal", timeout_seconds=timeout, minimum_free_bytes=4 * GIB,
                  private_limit_bytes=GIB)
    started = time.monotonic(); process = tree = None
    try:
        if result["minimum_available_bytes"] < 4 * GIB:
            raise RuntimeError("less than 4GiB available before launch")
        with log.open("wb") as output:
            process = subprocess.Popen(result["command"], cwd=ROOT, stdout=output, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW | subprocess.NORMAL_PRIORITY_CLASS | 4)
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
                result["peak_scratch_bytes"] = max(result["peak_scratch_bytes"], file_bytes(scratch, {".run", ".view"}))
                if result["minimum_available_bytes"] < 4 * GIB or memory.private > GIB:
                    raise RuntimeError("memory guard exceeded")
                if time.monotonic() - started > timeout:
                    raise RuntimeError("hierarchy cook deadline exceeded")
                time.sleep(.1)
            result["exit_code"] = process.wait(timeout=5)
            tree.verify_clean_exit(output)
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


def summarize(package, primitive):
    result = {"package_bytes": file_bytes(package.parent), "geometry_files": len(list(package.parent.rglob("*.vgeom")))}
    if not package.exists():
        return result
    header = json.loads(package.read_text(encoding="utf-8"))
    summary = header["primitives"][primitive]
    result.update(complete=header["complete"], primitive_complete=summary["complete"], published_roots=len(summary["roots"]),
                  original_nodes=len(header["instances"]), primitive_count=len(header["primitives"]),
                  source_hash=header["source_hash"], header_bytes=package.stat().st_size)
    shard = package.parent / summary["shard"] if summary["shard"] else package.parent / "work" / f"primitive-{primitive}.json"
    if shard.exists():
        nodes = json.loads(shard.read_text(encoding="utf-8"))["nodes"]
        leaves = [node for node in nodes if not node["children"]]
        result.update(completed_leaves=len(leaves), completed_source_triangles=sum(node["source_triangles"] for node in leaves),
                      nodes=len(nodes), leaf_coarse_triangles=sum(node["coarse"]["triangles"] for node in leaves),
                      leaf_root_pages=sum(len(node["coarse"]["root_page_ids"]) for node in leaves),
                      leaf_max_error=max((node["coarse"]["error"] for node in leaves), default=0))
    roots = summary["roots"]
    result.update(root_triangles=sum(node["coarse"]["triangles"] for node in roots),
                  root_pages=sum(len(node["coarse"]["root_page_ids"]) for node in roots),
                  root_payload_bytes=sum(node["coarse"]["page_used_bytes"][page] for node in roots for page in node["coarse"]["root_page_ids"]),
                  root_metadata_bytes=sum(node["coarse"]["metadata_bytes"] for node in roots),
                  root_max_error=max((node["coarse"]["error"] for node in roots), default=0))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--primitive", required=True, type=int)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--whole-primitive", action="store_true")
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--target-triangles", type=int, default=2048)
    args = parser.parse_args()
    if os.name != "nt" or not 0 < args.timeout <= 300:
        parser.error("Windows only; deadline must be positive and at most 300 seconds")
    if not 1 <= args.target_triangles <= 65536:
        parser.error("target triangles must be in 1..65536")
    catalog = args.catalog.resolve(); package = args.package.resolve(); output = args.output.resolve()
    if package == catalog or package.parent == catalog.parent:
        parser.error("hierarchy measurement requires a separate package directory")
    source = json.loads(catalog.read_text(encoding="utf-8")); primitive = source["primitives"][args.primitive]
    output.mkdir(parents=True, exist_ok=False)
    report = dict(catalog=str(catalog), package=str(package), primitive=args.primitive, mesh=primitive["mesh"],
                  source_primitive=primitive["primitive"], triangles=primitive["triangles"], parts=primitive["part_count"],
                  mode="whole-primitive" if args.whole_primitive else "one-window", target_triangles=args.target_triangles)
    canonical_hash = digest(catalog)
    report["source_sha256_before"] = {item["path"]: digest(Path(item["path"])) for item in source["resources"]}
    command = [COOK, "--hierarchy" if args.whole_primitive else "--hierarchy-window", catalog, package, args.primitive]
    if args.whole_primitive:
        command.append(1)
    command.append(args.target_triangles)
    report["cook"] = guarded(command, output / "cook.log", package.parent, args.timeout)
    report["summary"] = summarize(package, args.primitive)
    report["source_sha256_after"] = {item["path"]: digest(Path(item["path"])) for item in source["resources"]}
    report["source_unchanged"] = report["source_sha256_before"] == report["source_sha256_after"]
    report["canonical_unchanged"] = canonical_hash == digest(catalog)
    summary = report["summary"]
    expected = summary.get("primitive_complete") and summary.get("completed_source_triangles") == primitive["triangles"] if args.whole_primitive else summary.get("completed_leaves", 0) > 0
    report["passed"] = report["cook"]["status"] == "completed" and expected and report["source_unchanged"] and report["canonical_unchanged"]
    (output / "measurement.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
