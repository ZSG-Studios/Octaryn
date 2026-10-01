"""Prepare, run or inspect the bounded incoming-part authority/capture fixture."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from scene_stream_evidence import inspect
from scene_stream_fixture import generate


def identities(directory):
    catalog = json.loads((directory / "scene.json").read_text())
    paths = {directory / name for name in ("reflection.gltf", "reflection.bin", "map.json", "route.json", "scene.json")}
    for part in catalog["parts"]:
        if not part["geometry"] or not part["bounds_prepared"]:
            raise RuntimeError("Every authored fixture part must be prepared")
        paths.add(directory / part["geometry"])
    if (len(catalog["parts"]), catalog["unique_triangles"], catalog["instanced_triangles"], len(catalog["instances"])) != (9, 18, 18, 3):
        raise RuntimeError("Fixture source coverage changed")
    result = {}
    for path in sorted(paths):
        with path.open("rb") as stream:
            result[str(path.relative_to(directory))] = hashlib.file_digest(stream, "sha256").hexdigest()
    return result


def prepare(fixture, cook):
    generate(fixture)
    with (fixture / "cook.log").open("w", encoding="utf-8") as log:
        for arguments in (("--catalog", str(fixture / "reflection.gltf"), str(fixture / "scene.json")),
                          ("--cook", str(fixture / "scene.json"))):
            subprocess.run([str(cook), *arguments], stdout=log, stderr=subprocess.STDOUT, timeout=60, check=True,
                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    proof = dict(status="prepared", source_coverage="all 18 triangles, all 3 original nodes", identity=identities(fixture))
    (fixture / "prepared.json").write_text(json.dumps(proof, indent=2), encoding="utf-8")
    return proof


def run(args):
    fixture = args.fixture.resolve()
    before = identities(fixture)
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="scene-stream-", dir=args.evidence_root.resolve()))
    driver = Path(__file__).with_name("capture_map_world.py")
    command = [sys.executable, str(driver), "--client-bundle-root", str(args.client_bundle_root.resolve()),
        "--manifest", str(fixture / "map.json"), "--gameplay-route", str(fixture / "route.json"),
        "--evidence-root", str(root), "--backend", args.backend, "--scene-stream", "--scene-continuity",
        "--reflection-distance", "64", "--geometry-pixels", "0", "--frames", "1800",
        "--capture-min-frame", "180", "--captures", "24", "--stride", "60",
        "--width", "1280", "--height", "720", "--timeout", "150", "--max-frame-ms", "50"]
    if args.shader_cache_dir:
        command += ["--shader-cache-dir", str(args.shader_cache_dir.resolve())]
    result = dict(status="running", command=command, source_identity=before, watchdog_max_frame_ms=50,
                  render_radius=128, keep_radius=160, reflection_distance=64,
                  timing_qualification=False, scope="Incoming-part mechanism, not Zorah-scale acceptance")
    try:
        with (root / "driver.log").open("w", encoding="utf-8") as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=170, check=True,
                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        cases = list(root.glob("map-*/result.json"))
        if len(cases) != 1:
            raise RuntimeError("Capture produced an ambiguous evidence directory")
        result["case"] = str(cases[0].parent)
        result["evidence"] = inspect(cases[0].parent)
        if identities(fixture) != before:
            raise RuntimeError("Source or prepared geometry changed during the run")
        result.update(status="passed", source_unchanged=True)
    except Exception as failure:
        result.update(status="failed", error=str(failure))
        raise
    finally:
        (root / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(root, flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("prepare", "run", "inspect"), required=True)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--cook", type=Path)
    parser.add_argument("--case", type=Path)
    parser.add_argument("--client-bundle-root", type=Path)
    parser.add_argument("--evidence-root", type=Path)
    parser.add_argument("--shader-cache-dir", type=Path)
    parser.add_argument("--backend", choices=("dx12", "vulkan"), default="dx12")
    args = parser.parse_args()
    if args.mode == "prepare":
        if not args.fixture or not args.cook:
            parser.error("prepare requires --fixture and --cook")
        report = prepare(args.fixture.resolve(), args.cook.resolve())
    elif args.mode == "inspect":
        if not args.case:
            parser.error("inspect requires --case")
        report = inspect(args.case.resolve())
    else:
        if not args.fixture or not args.client_bundle_root or not args.evidence_root:
            parser.error("run requires --fixture, --client-bundle-root and --evidence-root")
        report = run(args)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
