#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys


DEFAULT_ROOTS = (
    "octaryn-shared/Source/Host",
    "octaryn-shared/Source/Networking",
    "octaryn-client/Source/HostBridge",
    "octaryn-server/Source/Managed",
)

DEFAULT_C_ABI_TYPES = "octaryn-shared/Source/Native/HostAbi/octaryn_shared_abi_types.h"

MANAGED_FIELD_PATTERNS = (
    "string ",
    "object ",
    "IReadOnlyList<",
    "IReadOnlyCollection<",
    "IEnumerable<",
    "List<",
    "Dictionary<",
    "HashSet<",
    "ReadOnlySpan<",
    "Span<",
    "Memory<",
    "ReadOnlyMemory<",
)


STRUCT_DECLARATION = re.compile(r"(?:public|internal)\s+(?:unsafe\s+)?(?:readonly\s+)?struct\s+([A-Za-z0-9_]+)")
STRUCT_LAYOUT_SIZE = re.compile(r"StructLayout\([^)]*Size\s*=\s*(\d+)")
SIZE_VALUE = re.compile(r"SizeValue\s*=\s*(\d+)u?")
STRUCT_LAYOUT = "[StructLayout(LayoutKind.Sequential"
CS_STRUCT_NAME = re.compile(r"(?:public|internal)\s+(?:unsafe\s+)?(?:readonly\s+)?struct\s+([A-Za-z0-9_]+)[^{]*{")
C_SIZE_MACRO = re.compile(r"#define\s+(OCTARYN_[A-Z0-9_]+_SIZE)\s+(\d+)u?")

MANAGED_STRUCT_SIZE_MACROS = {
    "HostCommand": "OCTARYN_HOST_COMMAND_SIZE",
    "HostInputSnapshot": "OCTARYN_HOST_INPUT_SNAPSHOT_SIZE",
    "HostFrameTimingSnapshot": "OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE",
    "HostFrameSnapshot": "OCTARYN_HOST_FRAME_SNAPSHOT_SIZE",
    "ClientCommandFrame": "OCTARYN_CLIENT_COMMAND_FRAME_SIZE",
    "ServerSnapshotHeader": "OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE",
    "ChunkColumnRequestFrame": "OCTARYN_CHUNK_COLUMN_REQUEST_FRAME_SIZE",
    "ChunkColumnSnapshotColumn": "OCTARYN_CHUNK_COLUMN_SNAPSHOT_COLUMN_SIZE",
    "ChunkColumnSnapshotBlock": "OCTARYN_CHUNK_COLUMN_SNAPSHOT_BLOCK_SIZE",
    "ReplicationChange": "OCTARYN_REPLICATION_CHANGE_SIZE",
    "NetworkMessageHeader": "OCTARYN_NETWORK_MESSAGE_HEADER_SIZE",
    "NativeHostApi": "OCTARYN_CLIENT_NATIVE_HOST_API_SIZE",
    "ServerNativeHostApi": "OCTARYN_SERVER_NATIVE_HOST_API_SIZE",
}


def iter_cs_files(roots):
    for root in roots:
        if not root.exists():
            continue
        yield from root.rglob("*.cs")


def is_native_facing(text):
    return STRUCT_LAYOUT in text


def load_c_size_macros(path):
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8")
    return {name: int(value) for name, value in C_SIZE_MACRO.findall(text)}


def validate_file(path, c_size_macros):
    text = path.read_text(encoding="utf-8")
    if not is_native_facing(text):
        return []

    errors = []
    if "Size =" not in text:
        errors.append(f"{path}: native-facing struct must declare explicit StructLayout Size")
    if "Pack = 8" not in text:
        errors.append(f"{path}: native-facing struct must use Pack = 8")

    declarations = STRUCT_DECLARATION.findall(text)
    if not declarations:
        errors.append(f"{path}: StructLayout file must declare a public or internal struct")

    for line_number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if not stripped.startswith("public ") or "(" in stripped:
            continue
        for pattern in MANAGED_FIELD_PATTERNS:
            if pattern in stripped:
                errors.append(f"{path}:{line_number}: native-facing field uses managed-only type pattern {pattern.strip()}")

    if "VersionValue" not in text:
        errors.append(f"{path}: native-facing struct must define VersionValue")
    if "SizeValue" not in text and "sizeof(" not in text:
        errors.append(f"{path}: native-facing struct must define SizeValue or be size-checked by sizeof")
    else:
        layout_size = STRUCT_LAYOUT_SIZE.search(text)
        size_value = SIZE_VALUE.search(text)
        if layout_size and size_value and layout_size.group(1) != size_value.group(1):
            errors.append(
                f"{path}: StructLayout Size {layout_size.group(1)} differs from SizeValue {size_value.group(1)}")
        struct_name = CS_STRUCT_NAME.search(text)
        if struct_name and size_value:
            macro_name = MANAGED_STRUCT_SIZE_MACROS.get(struct_name.group(1))
            c_size = c_size_macros.get(macro_name)
            if macro_name is None:
                errors.append(f"{path}: native-facing struct {struct_name.group(1)} has no C size macro mapping")
            elif c_size is None:
                errors.append(f"{path}: missing C size macro {macro_name}")
            elif c_size != int(size_value.group(1)):
                errors.append(
                    f"{path}: SizeValue {size_value.group(1)} differs from C macro {macro_name} {c_size}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--root", action="append", dest="roots")
    parser.add_argument("--c-abi-types", default=DEFAULT_C_ABI_TYPES)
    args = parser.parse_args()

    repo_root = pathlib.Path(args.repo_root).resolve()
    roots = [repo_root / root for root in (args.roots or DEFAULT_ROOTS)]
    c_size_macros = load_c_size_macros(repo_root / args.c_abi_types)

    errors = []
    for path in iter_cs_files(roots):
        errors.extend(validate_file(path, c_size_macros))

    if errors:
        for error in errors:
            print(f"native ABI contract policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
