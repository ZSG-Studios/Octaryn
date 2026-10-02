"""Provision the registry-pinned OpenUSD authoring SDK without changing system Python."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[3]
SDK = ROOT / "build/dependencies/python/openusd"


def ensure_sdk():
    registry = (ROOT / "cmake/Dependencies/DependencyRegistry.cmake").read_text(encoding="utf-8")
    entry = re.search(r"octaryn_register_dependency\(openusd_python\s+(.*?)\)", registry, re.S).group(1)
    url = re.search(r"\bURL (\S+)", entry).group(1)
    expected = re.search(r"\bURL_HASH SHA256=(\w+)", entry).group(1)
    tag = re.search(r"\bTAG (\S+)", entry).group(1)
    receipt = SDK / "sdk-info.json"
    if receipt.exists() and json.loads(receipt.read_text()).get("sha256") == expected and (SDK / "pxr/Usd").is_dir():
        return SDK
    if sys.version_info[:2] != (3, 12) or sys.platform != "win32":
        raise RuntimeError("The pinned USD SDK wheel currently qualifies Windows x64 / Python 3.12 only")
    cache = ROOT / "build/dependencies/downloads/openusd"
    cache.mkdir(parents=True, exist_ok=True)
    wheel = cache / url.rsplit("/", 1)[1]
    if not wheel.exists() or hashlib.file_digest(wheel.open("rb"), "sha256").hexdigest() != expected:
        temporary = wheel.with_suffix(".part")
        with urllib.request.urlopen(url, timeout=60) as source, temporary.open("wb") as destination:
            while data := source.read(1024 * 1024):
                destination.write(data)
        with temporary.open("rb") as stream:
            if hashlib.file_digest(stream, "sha256").hexdigest() != expected:
                raise RuntimeError("OpenUSD SDK wheel digest mismatch")
        temporary.replace(wheel)
    subprocess.run([sys.executable, "-m", "pip", "install", "--no-deps", "--no-compile", "--upgrade",
                    "--target", str(SDK), str(wheel)], check=True)
    receipt.write_text(json.dumps({"tag": tag, "sha256": expected, "python": "3.12", "platform": "win_amd64"}), encoding="utf-8")
    return SDK


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    print(f"openusd_sdk ready=1 path={ensure_sdk()}")
