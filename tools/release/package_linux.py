"""Package a frozen native Linux bundle; never build, launch or publish it."""

import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import tarfile


REQUIRED = (
    "Octaryn.Client", "Octaryn.Client.dll", "Octaryn.Client.runtimeconfig.json",
    "Octaryn.Basegame.dll", "Octaryn.Shared.dll",
    "Data/Module/octaryn.basegame.module.json", "liboctaryn_client_managed_bridge.so",
    "server/Octaryn.Server", "server/Octaryn.Server.dll",
    "server/Octaryn.Server.runtimeconfig.json", "server/Octaryn.Basegame.dll",
    "server/Octaryn.Shared.dll", "server/Data/Module/octaryn.basegame.module.json",
)
FORBIDDEN_DIRS = {"saves", "logs", "worlds", ".git", "__pycache__"}
FORBIDDEN_FILES = {"world_generation.json", "player_state.json", "inventory.json"}
LAUNCHER = '''#!/bin/sh
set -eu
cd -- "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
export OCTARYN_CLIENT_GRAPHICS_API=vulkan
export SDL_VIDEO_DRIVER=x11
exec ./Octaryn.Client "$@"
'''
START = '''# Native Linux preview

Install the matching architecture .NET 10 runtime (Microsoft.NETCore.App),
a Vulkan loader and working Vulkan GPU driver, and the system shared libraries
required by this build. The package is framework-dependent; it does not include
.NET or a graphics driver. Check the release notes for the tested distribution
and required system packages; a build on one distribution is not a portability
claim for every glibc version.

Extract the entire tar.gz with permissions preserved into a writable folder.
Run ./Launch-Octaryn.sh from a graphical Linux session. The launcher selects
Vulkan and X11: X11 or XWayland is required; native Wayland is not integrated.
The client starts its own packaged local authoritative server. Keep all libraries,
server/, Client/, Assets/ and Data/ together. Use Save & quit or close the client
for graceful server shutdown. The world continues while menus are open.

Standalone saves/logs use SDL's per-user ZSGStudios/Octaryn application-data
folder. OCTARYN_CLIENT_WORLD_PATH can select an absolute isolated world path.
The default world is saves/open-world-v3. Keep older saves separately; incompatible
or unversioned worlds are rejected. Do not run two clients against one world.

This is local play; internet/LAN multiplayer is unfinished. Packaging does not
qualify native or WSL graphics, sustained streaming, or a particular GPU.
See RELEASE_NOTES.md for actual build/runtime evidence and limitations.
THIRD_PARTY_NOTICES.txt and THIRD_PARTY/ retain dependency/asset attribution.
Static OpenAL Soft also requires the matching source/relink companion materials;
collecting notices or producing this archive alone does not supply them.
'''


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def regular(path, directory=False):
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
        raise ValueError(f"Links/reparse points are forbidden: {path}")
    valid = stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode)
    if not valid:
        raise ValueError(f"Unexpected filesystem object: {path}")
    return info


def scan(root, reject_user_data=False):
    regular(root, directory=True)
    files = {}
    for current, dirs, names in os.walk(root, followlinks=False):
        for name in dirs + names:
            path = Path(current) / name
            if reject_user_data and (name.casefold() in FORBIDDEN_DIRS | FORBIDDEN_FILES or path.suffix.lower() == ".log"):
                raise ValueError(f"Unexpected user/generated content: {path}")
            regular(path, directory=name in dirs)
        for name in sorted(names):
            path = Path(current) / name
            mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
            files[path.relative_to(root).as_posix()] = {
                "size": path.stat().st_size, "sha256": digest(path), "mode": mode,
            }
    return dict(sorted(files.items()))


def elf(path, machine):
    regular(path)
    with path.open("rb") as stream:
        header = stream.read(20)
    if (len(header) != 20 or header[:6] != b"\x7fELF\x02\x01"
            or int.from_bytes(header[18:20], "little") != machine):
        raise ValueError(f"Expected little-endian Linux ELF64 for selected architecture: {path}")
    if not path.stat().st_mode & 0o111:
        raise ValueError(f"Required executable lacks execute permission: {path}")


def merge(source, target, snapshot):
    for name, entry in snapshot.items():
        incoming, outgoing = source / name, target / name
        regular(incoming)
        outgoing.parent.mkdir(parents=True, exist_ok=True)
        if outgoing.exists():
            regular(outgoing)
            if digest(outgoing) != entry["sha256"]:
                raise ValueError(f"Content collision: {outgoing}")
        else:
            with incoming.open("rb") as src, outgoing.open("xb") as dst:
                shutil.copyfileobj(src, dst)
        outgoing.chmod(entry["mode"])
        if digest(outgoing) != entry["sha256"]:
            raise ValueError(f"Input changed during staging: {incoming}")


def write(path, text, executable=False):
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(text)
    path.chmod(0o755 if executable else 0o644)


def verify_archive(archive, name, expected):
    with tarfile.open(archive, "r:gz") as package:
        members = package.getmembers()
        if len(members) != len(expected) or {m.name for m in members} != {
                f"{name}/{p}" for p in expected}:
            raise ValueError("Archive member inventory mismatch")
        for member in members:
            entry = expected[member.name[len(name) + 1:]]
            if not member.isfile() or member.mode != entry["mode"] or member.size != entry["size"]:
                raise ValueError(f"Archive type/mode/size mismatch: {member.name}")
            value = hashlib.sha256()
            with package.extractfile(member) as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    value.update(block)
            if value.hexdigest() != entry["sha256"]:
                raise ValueError(f"Archive payload hash mismatch: {member.name}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("bundle", "notices", "output", "repo-root", "release-notes"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--architecture", choices=("x64", "arm64"), default="x64")
    parser.add_argument("--name", required=True, help="Simple directory/archive name without extension")
    args = parser.parse_args(argv)
    if os.name != "posix":
        raise ValueError("Package on native Linux/WSL to preserve executable permissions")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.name):
        raise ValueError("Invalid simple package name")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", args.source_commit):
        raise ValueError("Provide a full 40-character source commit")
    bundle, notices, output, repo = [p.absolute() for p in (
        args.bundle, args.notices, args.output, args.repo_root)]
    for path in (bundle, notices, repo):
        regular(path, directory=True)
    for path in (bundle, notices):
        if output.resolve().is_relative_to(path.resolve()):
            raise ValueError("Output must be outside bundle/notices inputs")
    before, attribution = scan(bundle, reject_user_data=True), scan(notices)
    for name in REQUIRED:
        if name not in before or before[name]["size"] == 0:
            raise ValueError(f"Required nonempty bundle file missing: {name}")
    for name in ("Octaryn.Client", "server/Octaryn.Server"):
        elf(bundle / name, 62 if args.architecture == "x64" else 183)
    if not any(p.startswith("Client/Shaders/") and p.endswith(".slang") for p in before):
        raise ValueError("Missing client Slang shaders")
    for prefix in ("", "server/"):
        name = "Octaryn.Server" if prefix else "Octaryn.Client"
        config = json.loads((bundle / f"{prefix}{name}.runtimeconfig.json").read_text())
        framework = config.get("runtimeOptions", {}).get("framework", {})
        if framework.get("name") != "Microsoft.NETCore.App" or not framework.get("version", "").startswith("10."):
            raise ValueError(f"Expected .NET 10 framework-dependent runtime config: {name}")
        json.loads((bundle / f"{prefix}Data/Module/octaryn.basegame.module.json").read_text())
    inventory = json.loads((notices / "THIRD_PARTY/inventory.json").read_text())
    if inventory.get("missing_required") != [] or "THIRD_PARTY_NOTICES.txt" not in attribution:
        raise ValueError("Incomplete notice collection")
    if inventory.get("platform") != f"linux-{args.architecture}":
        raise ValueError("Notices must identify the exact Linux architecture; Windows notices are not sufficient")
    for path in (repo / "LICENSE", repo / "README.md", args.release_notes):
        regular(path)
    stage, archive = output / args.name, output / f"{args.name}.tar.gz"
    checksum = output / f"{args.name}.tar.gz.sha256"
    if any(p.exists() or p.is_symlink() for p in (stage, archive, checksum)):
        raise ValueError("Output already exists; choose a fresh output directory/name")
    output.mkdir(parents=True, exist_ok=True)
    regular(output, directory=True)
    stage.mkdir(mode=0o755)
    merge(bundle, stage, before)
    merge(notices, stage, attribution)
    for source, target in ((repo / "LICENSE", "LICENSE"), (repo / "README.md", "README.md"),
                           (args.release_notes, "RELEASE_NOTES.md")):
        content = source.read_text(encoding="utf-8-sig")
        if target == "README.md":
            content = re.sub(r"\]\((docs/[^)]+)\)", lambda m:
                f"](https://github.com/ZSG-Studios/Octaryn/blob/{args.source_commit}/{m[1]})", content)
        dest = stage / target
        if dest.exists():
            if dest.read_text(encoding="utf-8-sig") != content:
                raise ValueError(f"Document collision: {target}")
        else:
            write(dest, content)
    write(stage / "Launch-Octaryn.sh", LAUNCHER, executable=True)
    write(stage / "GETTING_STARTED.md", START)
    manifest = {"schema": 1, "name": args.name, "platform": f"linux-{args.architecture}",
                "source_commit": args.source_commit.lower(), "created_utc": datetime.now(timezone.utc).isoformat(),
                "files": scan(stage), "manifest_note": "Payload SHA-256 and portable modes exclude manifest.json itself."}
    write(stage / "manifest.json", json.dumps(manifest, indent=2) + "\n")
    expected = scan(stage)
    with archive.open("xb") as raw, gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as package:
            for name, entry in expected.items():
                info = tarfile.TarInfo(f"{args.name}/{name}")
                info.size, info.mode = entry["size"], entry["mode"]
                info.uid = info.gid = info.mtime = 0
                info.uname = info.gname = ""
                with (stage / name).open("rb") as stream:
                    package.addfile(info, stream)
    verify_archive(archive, args.name, expected)
    if scan(bundle, reject_user_data=True) != before or scan(notices) != attribution:
        raise ValueError("Input bundle/notices changed during packaging")
    value = digest(archive)
    write(checksum, f"{value}  {archive.name}\n")
    print(json.dumps({"archive": str(archive), "sha256": value, "files": len(expected),
                      "runtime_qualification": "not performed by packaging"}))


if __name__ == "__main__":
    main()
