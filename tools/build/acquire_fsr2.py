"""Acquire immutable MIT FSR 2.2.1 sources; never replace conflicting files."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import urllib.request

GODOT = "2f698aa5fe31d0be68f205ec41aec9365081d364"
AMD = "1680d1edd5c034f88ebbbb793d8b88f8842cf804"


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
    args = parser.parse_args()
    destination = args.destination.resolve()
    files = inventory("godotengine/godot", GODOT, "thirdparty/amd-fsr2", destination / "godot")
    files += inventory("GPUOpen-Effects/FidelityFX-FSR2", AMD,
                       "src/ffx-fsr2-api/shaders", destination / "upstream/shaders")
    license_url = f"https://api.github.com/repos/godotengine/godot/contents/LICENSE.txt?ref={GODOT}"
    license_info = json.loads(fetch(license_url))
    files.append((destination / "GODOT-LICENSE.txt", license_info["download_url"], license_info["sha"]))
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        manifest = list(pool.map(acquire, files))
    notice = ("FSR 2.2.1: AMD MIT source with Godot MIT integration patches.\n"
              f"Godot commit {GODOT}\nAMD commit {AMD}\n"
              "Unmodified vendor files remain outside the first-party 500-line limit.\n"
              "See godot/LICENSE.txt and godot/patches for original notices and changes.\n")
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "PROVENANCE.txt").write_text(notice, encoding="utf-8")
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"fsr2_acquisition=passed files={len(files)} destination={destination}")


if __name__ == "__main__":
    main()
