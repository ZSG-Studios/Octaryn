"""Acquire immutable MIT FSR 2.2.1 sources; never replace conflicting files."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import urllib.request


def pins(root):
    registry = (root / "cmake/Dependencies/DependencyRegistry.cmake").read_text()
    fields = {}
    for name in ("fsr2_version", "fsr2_godot_repository", "fsr2_godot_commit",
                 "fsr2_amd_repository", "fsr2_amd_commit"):
        match = re.search(rf'set\(OCTARYN_DEP_{name} "([^"]+)"\)', registry)
        if not match:
            raise RuntimeError(f"Missing central registry pin: OCTARYN_DEP_{name}")
        fields[name] = match.group(1)
    return fields


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Octaryn-FSR2-pinned"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def inventory(repo, commit, remote, local):
    url = f"https://api.github.com/repos/{repo}/contents/{remote}?ref={commit}"
    entries = json.loads(fetch(url))
    result = []
    for entry in entries:
        destination = local / entry["name"]
        if entry["type"] == "dir":
            result.extend(inventory(repo, commit, entry["path"], destination))
        elif entry["type"] == "file":
            result.append((destination, entry["download_url"], entry["sha"]))
        else:
            raise RuntimeError(f"Unexpected upstream entry: {entry['path']}")
    return result


def acquire(item):
    path, url, expected = item
    data = path.read_bytes() if path.exists() else fetch(url)
    digest = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
    if digest != expected:
        raise RuntimeError(f"Immutable source mismatch; refusing replacement: {path}")
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(path.suffix + ".download")
        temporary.write_bytes(data)
        temporary.replace(path)
    return {"path": str(path), "url": url, "git_blob": expected,
            "sha256": hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", required=True, type=Path)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    pin = pins(args.repo.resolve())
    destination = args.destination.resolve()
    files = inventory(pin["fsr2_godot_repository"], pin["fsr2_godot_commit"],
                      "thirdparty/amd-fsr2", destination / "godot")
    files += inventory(pin["fsr2_amd_repository"], pin["fsr2_amd_commit"],
                       "src/ffx-fsr2-api/shaders", destination / "upstream/shaders")
    license_url = (f"https://api.github.com/repos/{pin['fsr2_godot_repository']}"
                   f"/contents/LICENSE.txt?ref={pin['fsr2_godot_commit']}")
    license_info = json.loads(fetch(license_url))
    files.append((destination / "GODOT-LICENSE.txt", license_info["download_url"], license_info["sha"]))
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        manifest = list(pool.map(acquire, files))
    notice = (f"FSR {pin['fsr2_version']}: AMD MIT source with Godot MIT integration patches.\n"
              f"Godot commit {pin['fsr2_godot_commit']}\nAMD commit {pin['fsr2_amd_commit']}\n"
              "Unmodified vendor files remain outside the first-party 500-line limit.\n"
              "See godot/LICENSE.txt and godot/patches for original notices and changes.\n")
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "PROVENANCE.txt").write_text(notice, encoding="utf-8")
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"fsr2_acquisition=passed files={len(files)} destination={destination}")


if __name__ == "__main__":
    main()
