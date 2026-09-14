#!/usr/bin/env python3
import argparse
import ctypes
from contextlib import ExitStack, contextmanager
import os
import pathlib
import sys


OWNER_FILES = {
    "client": {
        "assembly": "Octaryn.Client.dll",
        "runtimeconfig": "Octaryn.Client.runtimeconfig.json",
        "deps": "Octaryn.Client.deps.json",
        "functions": (
            "octaryn_client_initialize",
            "octaryn_client_tick",
            "octaryn_client_apply_server_snapshot",
            "octaryn_client_drain_presentation_updates",
            "octaryn_client_shutdown",
        ),
        "invalid_returns": (
            ("octaryn_client_initialize", ctypes.c_int, [ctypes.c_void_p], [None], -1),
            ("octaryn_client_tick", ctypes.c_int, [ctypes.c_void_p], [None], -1),
            ("octaryn_client_apply_server_snapshot", ctypes.c_int, [ctypes.c_void_p], [None], -1),
            (
                "octaryn_client_drain_presentation_updates",
                ctypes.c_int,
                [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p],
                [None, 0, None],
                -1,
            ),
        ),
    },
    "server": {
        "assembly": "Octaryn.Server.dll",
        "runtimeconfig": "Octaryn.Server.runtimeconfig.json",
        "deps": "Octaryn.Server.deps.json",
        "functions": (
            "octaryn_server_initialize",
            "octaryn_server_tick",
            "octaryn_server_submit_client_commands",
            "octaryn_server_drain_server_snapshots",
            "octaryn_server_request_chunk_columns",
            "octaryn_server_shutdown",
        ),
        "invalid_returns": (
            ("octaryn_server_initialize", ctypes.c_int, [ctypes.c_void_p], [None], -1),
            ("octaryn_server_tick", ctypes.c_int, [ctypes.c_void_p], [None], -1),
            ("octaryn_server_submit_client_commands", ctypes.c_int, [ctypes.c_void_p], [None], -1),
            ("octaryn_server_drain_server_snapshots", ctypes.c_int, [ctypes.c_void_p], [None], -1),
            ("octaryn_server_request_chunk_columns", ctypes.c_int, [ctypes.c_void_p], [None], -1),
        ),
    },
}


@contextmanager
def bridge_runtime(owner, bridge, bundle_dir):
    policy = OWNER_FILES[owner]
    prefix = f"OCTARYN_{owner.upper()}"
    overrides = {
        f"{prefix}_MANAGED_ASSEMBLY_PATH": str(bundle_dir / policy["assembly"]),
        f"{prefix}_RUNTIME_CONFIG_PATH": str(bundle_dir / policy["runtimeconfig"]),
    }
    previous = {name: os.environ.get(name) for name in overrides}
    try:
        os.environ.update(overrides)
        with ExitStack() as directories:
            if os.name == "nt":
                for directory in sorted({bridge.parent, bundle_dir}):
                    directories.enter_context(os.add_dll_directory(str(directory)))
            yield
    finally:
        for name, value in previous.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value


def validate(owner, bridge, bundle_dir):
    bridge = bridge.resolve()
    bundle_dir = bundle_dir.resolve()
    policy = OWNER_FILES[owner]
    errors = []

    for key in ("assembly", "runtimeconfig", "deps"):
        path = bundle_dir / policy[key]
        if not path.exists():
            errors.append(f"{path}: missing required {owner} {key}")

    if not bridge.exists():
        errors.append(f"{bridge}: missing native bridge library")

    if errors:
        return errors

    try:
        with bridge_runtime(owner, bridge, bundle_dir):
            library = ctypes.CDLL(str(bridge))
            return validate_exports(owner, bridge, library)
    except OSError as error:
        return [f"{bridge}: failed to load native bridge: {error}"]


def validate_exports(owner, bridge, library):
    policy = OWNER_FILES[owner]
    errors = []
    for function_name in policy["functions"]:
        try:
            getattr(library, function_name)
        except AttributeError:
            errors.append(f"{bridge}: missing exported function {function_name}")

    for function_name, restype, argtypes, args, expected in policy["invalid_returns"]:
        function = getattr(library, function_name, None)
        if function is None:
            continue
        function.restype = restype
        function.argtypes = argtypes
        actual = function(*args)
        if actual != expected:
            errors.append(
                f"{function_name}: expected managed invalid-input return {expected}, got {actual}")

    shutdown_name = "octaryn_client_shutdown" if owner == "client" else "octaryn_server_shutdown"
    shutdown = getattr(library, shutdown_name, None)
    if shutdown is not None:
        shutdown.restype = None
        shutdown.argtypes = []
        shutdown()

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--owner", choices=sorted(OWNER_FILES), required=True)
    parser.add_argument("--bridge", required=True)
    parser.add_argument("--bundle-dir", required=True)
    args = parser.parse_args()

    errors = validate(args.owner, pathlib.Path(args.bridge), pathlib.Path(args.bundle_dir))
    if errors:
        for error in errors:
            print(f"hostfxr bridge export policy: {error}", file=sys.stderr)
        return 1
    print(f"hostfxr_bridge_exports=passed owner={args.owner}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
