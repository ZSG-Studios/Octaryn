#!/usr/bin/env python3
import argparse
import filecmp
import pathlib
import stat
import sys


PAYLOAD_DIR = "server"

REQUIRED_SERVER_FILES = (
    "Octaryn.Server.dll",
    "Octaryn.Server.deps.json",
    "Octaryn.Server.runtimeconfig.json",
    "Octaryn.Shared.dll",
    "Octaryn.Basegame.dll",
    "Data/Module/octaryn.basegame.module.json",
)

SERVER_ENTRYPOINT_FILES = (
    "Octaryn.Server",
    "Octaryn.Server.exe",
)

FORBIDDEN_CLIENT_PAYLOADS = (
    "Octaryn.Client.dll",
    "Octaryn.Client.deps.json",
    "Octaryn.Client.runtimeconfig.json",
    "Client",
)


def relative_files(root):
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file()
    }


def existing_entrypoints(root):
    return [
        relative
        for relative in SERVER_ENTRYPOINT_FILES
        if (root / relative).is_file()
    ]


def validate_entrypoint_permissions(root, entrypoints, label, errors):
    for relative in entrypoints:
        entrypoint = root / relative
        if relative.endswith(".exe"):
            continue
        mode = entrypoint.stat().st_mode
        if not mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH):
            errors.append(
                f"{entrypoint}: {label} server entrypoint is not executable")


def validate(client_bundle_root, server_bundle_root):
    payload_root = client_bundle_root / PAYLOAD_DIR
    errors = []

    if not client_bundle_root.exists():
        errors.append(f"{client_bundle_root}: client bundle root is missing")
    if not server_bundle_root.exists():
        errors.append(f"{server_bundle_root}: server bundle root is missing")
    if not payload_root.exists():
        errors.append(f"{payload_root}: bundled server app is missing")
    if errors:
        return errors

    for relative in REQUIRED_SERVER_FILES:
        if not (server_bundle_root / relative).is_file():
            errors.append(f"{server_bundle_root / relative}: required server bundle file is missing")
        if not (payload_root / relative).is_file():
            errors.append(f"{payload_root / relative}: required bundled server file is missing")

    server_entrypoints = existing_entrypoints(server_bundle_root)
    payload_entrypoints = existing_entrypoints(payload_root)
    if not server_entrypoints:
        errors.append(f"{server_bundle_root}: dedicated server bundle has no launchable server entrypoint")
    if not payload_entrypoints:
        errors.append(f"{payload_root}: bundled server app has no launchable server entrypoint")
    if server_entrypoints != payload_entrypoints:
        errors.append(
            f"{payload_root}: bundled server entrypoints {payload_entrypoints} "
            f"do not match server-owned entrypoints {server_entrypoints}")
    validate_entrypoint_permissions(
        server_bundle_root, server_entrypoints, "dedicated", errors)
    validate_entrypoint_permissions(
        payload_root, payload_entrypoints, "bundled", errors)

    for relative in FORBIDDEN_CLIENT_PAYLOADS:
        if (server_bundle_root / relative).exists():
            errors.append(f"{server_bundle_root / relative}: dedicated server bundle contains client payload")
        if (payload_root / relative).exists():
            errors.append(f"{payload_root / relative}: bundled server app contains client payload")

    server_files = relative_files(server_bundle_root)
    payload_files = relative_files(payload_root)
    missing = sorted(server_files - payload_files)
    extra = sorted(payload_files - server_files)
    if missing:
        errors.append(f"{payload_root}: missing server-owned files: {missing}")
    if extra:
        errors.append(f"{payload_root}: contains files not present in server bundle: {extra}")

    for relative in sorted(server_files & payload_files):
        server_file = server_bundle_root / relative
        payload_file = payload_root / relative
        if not filecmp.cmp(server_file, payload_file, shallow=False):
            errors.append(f"{payload_file}: does not match server-owned source {server_file}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-bundle-root", required=True)
    parser.add_argument("--server-bundle-root", required=True)
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.client_bundle_root).resolve(),
        pathlib.Path(args.server_bundle_root).resolve())
    if errors:
        for error in errors:
            print(f"client server app policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
