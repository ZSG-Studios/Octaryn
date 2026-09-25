"""Narrow exception for the pinned MIT FSR2 includes, never first-party HLSL."""
import hashlib
import json
from pathlib import Path

GODOT = "2f698aa5fe31d0be68f205ec41aec9365081d364"
AMD = "1680d1edd5c034f88ebbbb793d8b88f8842cf804"


def vendor_root(repo):
    return Path(repo) / "build/dependencies/fsr2-2.2.1-godot-2f698aa5/slang"


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
