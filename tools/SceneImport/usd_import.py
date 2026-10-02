"""Compose OpenUSD at import time and emit a closed static Octaryn scene package."""
import argparse
import json
import math
from pathlib import Path
import re
import shutil
import sys
import tempfile

ENGINE = Path(__file__).resolve().parents[2]


def setup_sdk(sdk_root=None):
    sdk = Path(sdk_root) if sdk_root else ENGINE / "build/dependencies/python/openusd"
    if not (sdk / "pxr/Usd").is_dir():
        raise RuntimeError("Provision the registry-pinned SDK with tools/build/support/openusd_sdk.py")
    sys.path.insert(0, str(sdk.resolve()))
    from pxr import Usd
    registry = (ENGINE / "cmake/Dependencies/DependencyRegistry.cmake").read_text(encoding="utf-8")
    entry = re.search(r"octaryn_register_dependency\(openusd_python\s+(.*?)\)",registry,re.S)
    tag = re.search(r"\bTAG v(\d+)\.(\d+)",entry.group(1)) if entry else None
    if not tag or Usd.GetVersion() != (0,int(tag[1]),int(tag[2])):
        raise RuntimeError("OpenUSD SDK version does not match DependencyRegistry.cmake")
    return Usd.GetVersion()


def inspect(source, populations=()):
    from usd_scene import open_stage, metadata
    return metadata(open_stage(Path(source).resolve(strict=True), populations))


def cook(source, output, populations=(), payloads="all", time_value=None, budget=None,
         purposes=("default","render")):
    from usd_scene import open_stage, metadata, cook_stage
    from usd_support import Budget, require, digest
    source = Path(source).resolve(strict=True)
    output = Path(output).resolve()
    require(source.suffix.lower() in (".usd", ".usda", ".usdc"), "Expected an OpenUSD stage file")
    require(not output.exists() or (output.is_dir() and not any(output.iterdir())), "Use a new or empty output directory")
    require(output != source.parent and source != output, "Scene output must not replace source data")
    budget = budget or Budget()
    require(time_value is None or math.isfinite(time_value), "USD snapshot time must be finite")
    budget.check()
    source_hashes = {source:digest(source,budget)}
    def capture_sources(composed):
        for layer in composed.GetUsedLayers():
            if layer.realPath:
                path = Path(layer.realPath).resolve(strict=True)
                current = digest(path,budget)
                require(path not in source_hashes or current == source_hashes[path], "USD source layer changed during cooking")
                source_hashes[path] = current
    stage = open_stage(source, populations)
    capture_sources(stage)
    initial = metadata(stage)
    output.parent.mkdir(parents=True, exist_ok=True)
    pending = Path(tempfile.mkdtemp(prefix=f".{output.name}.pending-", dir=output.parent))
    try:
        gltf, counts, settings = cook_stage(stage, pending, budget, payloads, time_value, purposes, capture_sources)
        (pending / "scene.bin").write_bytes(gltf.binary)
        scene_text = json.dumps(gltf.document, indent=2, allow_nan=False).encode("utf-8")
        budget.charge(max(0,len(scene_text)-gltf.metadata_reservation))
        (pending / "scene.gltf").write_bytes(scene_text)
        files = []
        for path in sorted(pending.rglob("*")):
            if path.is_file():
                files.append({"path":path.relative_to(pending).as_posix(), "sha256":digest(path,budget), "bytes":path.stat().st_size})
        provenance = []
        source_hashes.update(gltf.source_images)
        for path, expected in sorted(source_hashes.items()):
            require(digest(path,budget) == expected, "USD source resource changed during cooking")
            provenance.append({"path":str(path), "sha256":expected, "bytes":path.stat().st_size})
        descriptor = {"version":1, "format":"openusd", "prepared":True, "scene":"scene.gltf", "files":files,
                      "source":{"path":str(source), "sha256":source_hashes[source], "usdVersion":initial["usdVersion"]},
                      "sources":provenance, "counts":counts, "initialMetadata":initial, **settings}
        text = json.dumps(descriptor, indent=2, allow_nan=False).encode("utf-8")
        budget.charge(len(text)); budget.check()
        (pending / "scene-import.json").write_bytes(text)
        if output.exists(): output.rmdir()
        pending.rename(output)
        return descriptor
    finally:
        # Only remove this tool's unique temporary directory, confined to the output parent.
        if pending.exists():
            require(pending.resolve().parent == output.parent.resolve() and pending.name.startswith(f".{output.name}.pending-"),
                    "Unexpected import temporary-directory path")
            shutil.rmtree(pending)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--metadata", action="store_true")
    parser.add_argument("--population", action="append", default=[])
    parser.add_argument("--payloads", choices=("all","none"), default="all")
    parser.add_argument("--purpose", action="append", choices=("default","render","proxy","guide"))
    parser.add_argument("--time", type=float)
    parser.add_argument("--max-bytes", type=int, default=256*1024*1024)
    parser.add_argument("--max-vertices", type=int, default=2_000_000)
    parser.add_argument("--max-instances", type=int, default=1_000_000)
    parser.add_argument("--cancel-file", type=Path)
    args = parser.parse_args()
    try:
        setup_sdk(args.sdk_root)
        if args.metadata:
            if args.out: parser.error("--metadata does not write a cooked output")
            print(json.dumps(inspect(args.source,args.population), indent=2, allow_nan=False))
        else:
            if not args.out: parser.error("--out is required for an explicit cook")
            from usd_support import Budget, require
            require(args.max_bytes > 0 and args.max_vertices > 0 and args.max_instances > 0, "Import limits must be positive")
            budget = Budget(args.max_bytes,args.max_vertices,args.max_instances,args.cancel_file)
            result = cook(args.source,args.out,args.population,args.payloads,args.time,budget,
                          tuple(args.purpose or ("default","render")))
            print(json.dumps({"usd_import":"CPUPrepared", "gpuPublished":False, "output":str(args.out), "counts":result["counts"]}))
        return 0
    except (Exception, KeyboardInterrupt) as failure:
        print(f"usd_import_failed reason={failure}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
