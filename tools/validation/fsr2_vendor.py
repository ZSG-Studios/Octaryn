"""Narrow exception for the pinned MIT FSR2 includes, never first-party HLSL."""
import hashlib
import json
import re
from pathlib import Path

REGISTRY = Path(__file__).resolve().parents[2] / "cmake/Dependencies/DependencyRegistry.cmake"


def _pin(name):
    match = re.search(rf'set\(OCTARYN_DEP_fsr2_{name} "([^"]+)"\)',
                      REGISTRY.read_text(encoding="utf-8"))
    if not match:
        raise ValueError(f"DependencyRegistry.cmake lacks OCTARYN_DEP_fsr2_{name}")
    return match.group(1)


GODOT = _pin("godot_commit")
AMD = _pin("amd_commit")


def vendor_root(repo):
    return Path(repo) / "build/dependencies" / _pin("pin") / "slang"


def verified_files(root):
    root = Path(root)
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("godot") != GODOT or manifest.get("amd") != AMD:
        raise ValueError("FSR2 vendor revisions are not the approved immutable pins")
    entries = manifest["files"]
    if not {"AMD-LICENSE.txt", "GODOT-LICENSE.txt", "PROVENANCE.txt"} <= entries.keys():
        raise ValueError("FSR2 license/provenance files missing")
    for name, digest in entries.items():
        if Path(name).name != name or name.endswith(".glsl"):
            raise ValueError("Invalid FSR2 vendor filename")
        if not (name.startswith("ffx_") and name.endswith((".h", ".hlsl")) or
                name in ("AMD-LICENSE.txt", "GODOT-LICENSE.txt", "PROVENANCE.txt")):
            raise ValueError("Undeclared non-FSR2 vendor file")
        if hashlib.sha256((root / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"FSR2 vendor content mismatch: {name}")
    actual = {p.name for p in root.iterdir() if p.is_file()}
    if actual != set(entries) | {"manifest.json"} or any(p.is_dir() for p in root.iterdir()):
        raise ValueError("FSR2 vendor tree contains undeclared files")
    return set(entries) | {"manifest.json"}
