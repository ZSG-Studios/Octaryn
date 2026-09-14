"""Package an already frozen Windows client/server bundle without building it."""

import argparse
import hashlib
import json
import os
import posixpath
from pathlib import Path
import re
import shutil
import stat
import sys
from datetime import datetime, timezone
import zipfile
from urllib.parse import quote, unquote, urlsplit


DEFAULT_NAME = "octaryn-slang-rhi-preview-windows-x64-20260914"
NOTES = "docs/releases/2026-09-14-slang-rhi-preview.md"
REQUIRED = (
    "Octaryn.Client.exe", "Octaryn.Client.dll",
    "Octaryn.Client.runtimeconfig.json", "Octaryn.Basegame.dll",
    "Data/Module/octaryn.basegame.module.json",
    "server/Octaryn.Server.exe", "server/Octaryn.Server.dll",
    "server/Octaryn.Server.runtimeconfig.json", "server/Octaryn.Basegame.dll",
    "server/Data/Module/octaryn.basegame.module.json",
)
FORBIDDEN_DIRS = {"saves", "logs", "worlds", ".git"}
FORBIDDEN_FILES = {"world_generation.json", "player_state.json", "inventory.json"}


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def reject_link(path):
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
        raise ValueError(f"Links/reparse points are not allowed: {path}")


def scan(root, reject_user_data=False):
    reject_link(root)
    result = {}
    for current, dirs, files in os.walk(root, followlinks=False):
        for name in dirs + files:
            path = Path(current) / name
            reject_link(path)
            if reject_user_data and (
                name.casefold() in FORBIDDEN_DIRS
                or name.casefold() in FORBIDDEN_FILES
                or path.suffix.casefold() == ".log"
            ):
                raise ValueError(f"Unexpected save/log content in bundle: {path}")
        for name in sorted(files):
            path = Path(current) / name
            if not path.is_file():
                raise ValueError(f"Not a regular file: {path}")
            key = path.relative_to(root).as_posix()
            result[key] = {"size": path.stat().st_size, "sha256": digest(path)}
    return dict(sorted(result.items()))


def copy_tree(source, destination):
    """Merge only into vacant paths; never replace bundle content."""
    reject_link(source)
    for current, dirs, files in os.walk(source, followlinks=False):
        relative = Path(current).relative_to(source)
        target = destination / relative
        target.mkdir(parents=True, exist_ok=True)
        for name in dirs + files:
            reject_link(Path(current) / name)
        for name in files:
            output = target / name
            if output.exists():
                if output.is_file() and digest(output) == digest(Path(current) / name):
                    continue
                raise ValueError(f"Package content collision: {output}")
            shutil.copy2(Path(current) / name, output)


def write_new(path, content):
    with path.open("x", encoding="utf-8", newline="") as stream:
        stream.write(content)


def document_links(content, source, repo, commit):
    """Resolve inline relative Markdown links against the original repo document."""
    parent = source.resolve().relative_to(repo.resolve()).parent.as_posix()

    def replace(match):
        target = match.group(1)
        destination = target[1:-1] if target.startswith("<") else target
        parsed = urlsplit(destination)
        if parsed.scheme or parsed.netloc or destination.startswith(("#", "/")):
            return match.group(0)
        if not parsed.path:
            return match.group(0)
        relative = posixpath.normpath(posixpath.join(parent, unquote(parsed.path)))
        if relative == ".." or relative.startswith("../"):
            raise ValueError(f"Document link leaves repository: {source}: {destination}")
        url = "https://github.com/ZSG-Studios/Octaryn/blob/" + commit + "/" + quote(relative, safe="/")
        if parsed.query:
            url += "?" + parsed.query
        if parsed.fragment:
            url += "#" + parsed.fragment
        return "](" + url + (match.group(2) or "") + ")"

    return re.sub(r'\]\((<[^>]+>|[^\s)]+)(\s+"[^"]*")?\)', replace, content)

def launcher(api):
    return (
        "@echo off\r\nsetlocal\r\ncd /d \"%~dp0\"\r\n"
        f"set \"OCTARYN_CLIENT_GRAPHICS_API={api}\"\r\n"
        '"%~dp0Octaryn.Client.exe"\r\n'
        'set "OCTARYN_EXIT_CODE=%ERRORLEVEL%"\r\n'
        'if not "%OCTARYN_EXIT_CODE%"=="0" pause\r\n'
        "exit /b %OCTARYN_EXIT_CODE%\r\n"
    )


GETTING_STARTED = """# Getting started

1. Install the Windows x64 .NET 10 Runtime (Microsoft.NETCore.App):
   https://dotnet.microsoft.com/en-us/download/dotnet/10.0
2. Install the Microsoft Visual C++ x64 Redistributable:
   https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist
3. Extract the whole ZIP into a writable folder. Keep server/, Client/, Assets/,
   Data/ and all DLLs together. Do not run from inside the ZIP viewer.
4. Run Launch-Octaryn.cmd for DirectX 12, or Launch-Vulkan.cmd for Vulkan.
   The client automatically starts its packaged local authoritative server.
   You do not need to start the server executable separately.

Use a current GPU driver. Windows/Radeon RX 9070 XT development validation does
not establish minimum hardware specifications or support for every GPU.

Escape opens the menu; use Save & quit to finish the local session. I or E opens
inventory, B opens Creative blocks, 1-0 selects hotbar slots, and T tosses an item.
The world continues simulating while menus are open.

Standalone installs normally store saves and logs under the SDL per-user
ZSGStudios/Octaryn application-data directory (normally
%APPDATA%\\ZSGStudios\\Octaryn on Windows). The default world is
saves/open-world-v2 there. OCTARYN_CLIENT_WORLD_PATH overrides the world location.
Do not run multiple clients against the same world. Keep earlier release saves
separate: incompatible/unversioned worlds are rejected, not migrated silently.

This preview provides local play, not internet/LAN multiplayer. See
RELEASE_NOTES.md for capabilities, validation limits and world compatibility;
see THIRD_PARTY_NOTICES.txt and THIRD_PARTY/ for bundled dependency notices.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--notices", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--name", default=DEFAULT_NAME)
    parser.add_argument("--release-notes", type=Path, default=Path(NOTES),
                        help="Release notes path, relative to --repo-root unless absolute")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.name):
        raise ValueError("--name must be a simple archive/directory name")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", args.source_commit):
        raise ValueError("--source-commit must be a full 40-character Git commit")
    bundle, repo, notices, output = (
        p.absolute() for p in (args.bundle, args.repo_root, args.notices, args.output)
    )
    notes = (repo / args.release_notes).resolve()
    if not notes.is_relative_to(repo.resolve()):
        raise ValueError("--release-notes must be inside --repo-root for immutable source links")
    for directory in (bundle, repo, notices):
        if not directory.is_dir():
            raise ValueError(f"Missing input directory: {directory}")
        reject_link(directory)
    for source in (bundle, notices):
        if output.resolve().is_relative_to(source.resolve()):
            raise ValueError("Output must be outside input bundle and notices trees")
    before = scan(bundle, reject_user_data=True)
    for name in REQUIRED:
        if name not in before or before[name]["size"] == 0:
            raise ValueError(f"Required bundle content missing/empty: {name}")
    if not any(p.startswith("Client/Shaders/") and p.endswith(".slang") for p in before):
        raise ValueError("Client Slang shader tree missing")
    for name in ("Data/Module/octaryn.basegame.module.json",
                 "server/Data/Module/octaryn.basegame.module.json"):
        if not isinstance(json.loads((bundle / name).read_text(encoding="utf-8-sig")), dict):
            raise ValueError(f"Invalid module metadata: {name}")
    notice_snapshot = scan(notices)
    if "THIRD_PARTY_NOTICES.txt" not in notice_snapshot or not any(
        name.startswith("THIRD_PARTY/") for name in notice_snapshot
    ):
        raise ValueError("Notices tree requires THIRD_PARTY_NOTICES.txt and THIRD_PARTY/ files")
    notice_inventory = json.loads((notices / "THIRD_PARTY/inventory.json").read_text(encoding="utf-8"))
    if notice_inventory.get("missing_required") != []:
        raise ValueError("Notice collection is incomplete; inspect THIRD_PARTY/inventory.json")
    for name in ("LICENSE", "README.md", args.release_notes):
        reject_link(repo / name)
        if not (repo / name).is_file():
            raise ValueError(f"Missing repository document: {name}")
    stage = output / args.name
    archive = output / f"{args.name}.zip"
    checksum = output / f"{args.name}.zip.sha256"
    if any(path.exists() for path in (stage, archive, checksum)):
        raise ValueError("Output stage, ZIP or checksum already exists; choose a fresh output")
    output.mkdir(parents=True, exist_ok=True)
    stage.mkdir()
    copy_tree(bundle, stage)
    if scan(stage) != before:
        raise ValueError("Staged bundle differs from initial source snapshot")
    copy_tree(notices, stage)
    for source, target in (("LICENSE", "LICENSE"), ("README.md", "README.md"),
                           (args.release_notes, "RELEASE_NOTES.md")):
        content = (repo / source).read_text(encoding="utf-8-sig")
        if target in ("README.md", "RELEASE_NOTES.md"):
            content = document_links(content, repo / source, repo, args.source_commit)
        destination = stage / target
        if destination.exists():
            if destination.read_text(encoding="utf-8-sig") != content:
                raise ValueError(f"Document content collision: {target}")
        else:
            write_new(destination, content)
    write_new(stage / "GETTING_STARTED.md", GETTING_STARTED)
    write_new(stage / "Launch-Octaryn.cmd", launcher("dx12"))
    write_new(stage / "Launch-Vulkan.cmd", launcher("vulkan"))
    payload = scan(stage)
    manifest = {
        "schema": 1, "name": args.name, "platform": "windows-x64",
        "source_commit": args.source_commit.lower(),
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "files": payload,
        "manifest_note": "Hashes cover every payload file except manifest.json itself.",
    }
    write_new(stage / "manifest.json", json.dumps(manifest, indent=2) + "\n")
    archived_files = scan(stage)
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as package:
        for name in archived_files:
            package.write(stage / name, f"{args.name}/{name}")
    with zipfile.ZipFile(archive) as package:
        if package.testzip() is not None:
            raise ValueError("ZIP CRC verification failed")
        if set(package.namelist()) != {f"{args.name}/{name}" for name in archived_files}:
            raise ValueError("ZIP member inventory mismatch")
        for name, entry in archived_files.items():
            value = hashlib.sha256()
            with package.open(f"{args.name}/{name}") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    value.update(block)
            if value.hexdigest() != entry["sha256"]:
                raise ValueError(f"ZIP payload hash mismatch: {name}")
    if scan(bundle, reject_user_data=True) != before or scan(notices) != notice_snapshot:
        raise ValueError("Input bundle or notices changed during packaging; do not publish")
    write_new(checksum, f"{digest(archive)}  {archive.name}\n")
    print(json.dumps({"archive": str(archive), "checksum": str(checksum),
                      "stage": str(stage), "files": len(archived_files)}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f"Packaging failed: {error}", file=sys.stderr)
        sys.exit(1)
