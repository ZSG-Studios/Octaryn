"""Run native lazy-menu validation with deny-read locks on actual map payloads."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from validate_world_library import fixtures


def fixture(directory):
    fixtures(directory)
    maps = directory / "automatic/octaryn-client/Assets/Maps"
    maps.mkdir(parents=True)
    sources = directory / "sources"
    shutil.copy2(sources / "0/main.glb", maps / "main.glb")
    (maps / "map.json").write_text(json.dumps(dict(version=1, map="main.glb",
        spawn=[0, 1.62, 0], yaw=0, pitch=0)), encoding="utf-8")
    shutil.copytree(sources / "1", maps / "second")
    shutil.copytree(sources / "external", maps / "external")
    (maps / "broken").mkdir()
    shutil.copy2(sources / "broken.glb", maps / "broken/broken.glb")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, required=True)
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="lazy-", dir=args.evidence_root.resolve()))
    fixture(directory)
    command = [str(args.probe.resolve()), "--lazy", str(directory)]
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
    text = result.stdout + result.stderr
    (directory / "probe.log").write_text(text, encoding="utf-8")
    report = dict(status="failed", command=command, exit_code=result.returncode,
                  source_access_scope="Native parse/hash/model/prepared-catalog counters plus Windows deny-read payload locks")
    try:
        result.check_returncode()
        for stage in ("unselected", "saved_menu"):
            marker = f"world_library_io phase={stage} source_parses=0 resource_hashes=0 model_loads=0 prepared_catalog_reads=0"
            if marker not in text:
                raise RuntimeError("Missing zero-content-access evidence for " + stage)
        if "world_library_lazy=passed" not in text:
            raise RuntimeError("Lazy selection/save regression did not pass")
        report["status"] = "passed"
    finally:
        (directory / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(text, end="")
        print(directory)


if __name__ == "__main__":
    main()
