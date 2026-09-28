#!/usr/bin/env python3
"""Verify the mirrored LiteEntitySystem session wire classes stay in parity.

The client and server compile separate copies of SessionEntity.cs: same wire
class, different host-side bodies. LiteEntitySystem assigns sync-var and RPC
ids purely by declaration/registration order, so both copies must declare the
same SyncVar fields in the same order and run the same RegisterRPC
registration calls in the same order. The mirrored SessionController.cs pair
must stay byte-identical because it carries the request wire structs.

Exits 0 when every mirror matches, 1 with a per-mismatch report otherwise.
"""
import argparse
import difflib
from itertools import zip_longest
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
CLIENT_ENTITY = "octaryn-client/Source/Host/Remote/SessionEntity.cs"
SERVER_ENTITY = "octaryn-server/Source/Networking/Remote/SessionEntity.cs"
CLIENT_CONTROLLER = "octaryn-client/Source/Host/Remote/SessionController.cs"
SERVER_CONTROLLER = "octaryn-server/Source/Networking/Remote/SessionController.cs"

SYNCVAR = re.compile(
    r"(?:\[\s*SyncVarFlags\(([^)]*)\)\s*\]\s*)?"
    r"(?:private|internal|protected|public)\s+SyncVar<([^>]+)>\s+(\w+)\s*;")
RPC_CALL = re.compile(r"\b\w+\.CreateRPC\w+\s*\((.*?)\)\s*;", re.S)
CLASS = re.compile(r"\b(?:class|struct)\s+(\w+)[^{;]*\{")
MAX_CONTROLLER_DIFF_LINES = 60


def strip_comments(text):
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)


def format_syncvar(entry):
    flags, var_type, name = entry
    return f"SyncVar<{var_type}> {name} [flags={flags or 'none'}]"


def extract_wire_classes(path):
    """Return {class name: (ordered SyncVar entries, ordered RPC calls)}."""
    text = strip_comments(path.read_text(encoding="utf-8-sig"))
    layout = {}
    for match in CLASS.finditer(text):
        body = match_class_body(text, match)
        if body is None:
            continue
        syncvars = [
            ((flags or "").strip(), var_type.strip(), name)
            for flags, var_type, name in SYNCVAR.findall(body)
        ]
        rpcs = [re.sub(r"\s+", "", call) for call in RPC_CALL.findall(body)]
        if syncvars or rpcs:
            layout[match[1]] = (syncvars, rpcs)
    return layout


def match_class_body(text, match):
    start = match.end() - 1
    depth = 0
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:index]
    return None


def compare_sequence(class_name, kind, client_items, server_items):
    errors = []
    for index, (client_item, server_item) in enumerate(
            zip_longest(client_items, server_items), start=1):
        if client_item == server_item:
            continue
        client_text = client_item if isinstance(client_item, str) else format_syncvar(client_item)
        server_text = server_item if isinstance(server_item, str) else format_syncvar(server_item)
        errors.append(
            f"{class_name} {kind} #{index} differs:\n"
            f"  client {CLIENT_ENTITY}: {client_text}\n"
            f"  server {SERVER_ENTITY}: {server_text}")
    return errors


def compare_entities(client_layout, server_layout):
    errors = []
    for name in sorted(set(client_layout) | set(server_layout)):
        if name not in client_layout or name not in server_layout:
            missing_from = CLIENT_ENTITY if name not in client_layout else SERVER_ENTITY
            errors.append(f"wire class {name} declared on one side only; missing from {missing_from}")
            continue
        client_syncvars, client_rpcs = client_layout[name]
        server_syncvars, server_rpcs = server_layout[name]
        errors.extend(compare_sequence(name, "SyncVar declaration", client_syncvars, server_syncvars))
        errors.extend(compare_sequence(name, "RegisterRPC call", client_rpcs, server_rpcs))
    return errors


def compare_controllers(client_path, server_path):
    if client_path.read_bytes() == server_path.read_bytes():
        return []
    client_lines = client_path.read_text(encoding="utf-8-sig").splitlines()
    server_lines = server_path.read_text(encoding="utf-8-sig").splitlines()
    diff = list(difflib.unified_diff(
        client_lines, server_lines,
        fromfile=str(client_path), tofile=str(server_path), lineterm=""))
    trimmed = diff[:MAX_CONTROLLER_DIFF_LINES]
    if len(diff) > MAX_CONTROLLER_DIFF_LINES:
        trimmed.append(f"... {len(diff) - MAX_CONTROLLER_DIFF_LINES} more diff lines")
    return ["mirrored SessionController.cs files are not byte-identical:", *trimmed]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=ROOT)
    args = parser.parse_args()
    root = args.repo_root

    client_entity_path = root / CLIENT_ENTITY
    server_entity_path = root / SERVER_ENTITY
    for path in (client_entity_path, server_entity_path,
                 root / CLIENT_CONTROLLER, root / SERVER_CONTROLLER):
        if not path.is_file():
            print(f"session entity parity: missing mirrored source {path}", file=sys.stderr)
            return 1

    client_layout = extract_wire_classes(client_entity_path)
    server_layout = extract_wire_classes(server_entity_path)
    errors = compare_entities(client_layout, server_layout)
    errors.extend(compare_controllers(root / CLIENT_CONTROLLER, root / SERVER_CONTROLLER))

    if errors:
        for error in errors:
            print(f"session entity parity: {error}", file=sys.stderr)
        return 1

    wire_classes = len(client_layout)
    syncvar_count = sum(len(layout[0]) for layout in client_layout.values())
    rpc_count = sum(len(layout[1]) for layout in client_layout.values())
    print(f"session_entity_parity=passed classes={wire_classes} "
          f"syncvars={syncvar_count} rpcs={rpc_count} controller=byte-identical")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
