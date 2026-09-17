"""Check LES registration order across the client/server wire owners."""
import argparse
from pathlib import Path
import re


def layout(path):
    text = path.read_text(encoding="utf-8-sig")
    text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
    fields = re.findall(
        r"\[SyncVarFlags\(([^)]+)\)\]\s*private\s+SyncVar<([^>]+)>\s+(\w+)\s*;", text)
    calls = re.findall(r"private\s+static\s+(RemoteCall(?:Span)?<[^>]+>)\s+(\w+)\s*;", text)
    registrations = re.findall(r"\br\.CreateRPCAction\((.*?)\);", text, flags=re.S)
    registrations = [re.sub(r"\s+", "", value) for value in registrations]
    if not fields or not calls or len(calls) != len(registrations):
        raise RuntimeError(f"Incomplete LES wire declarations in {path}")
    return fields, calls, registrations


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    root = parser.parse_args().repo_root
    client = layout(root / "octaryn-client/Source/Host/Remote/SessionEntity.cs")
    server = layout(root / "octaryn-server/Source/Networking/Remote/SessionEntity.cs")
    for name, left, right in zip(("sync fields", "RPC types", "RPC registration order"), client, server):
        if left != right:
            raise RuntimeError(f"Client/server LES {name} differ:\nclient={left}\nserver={right}")
    print(f"remote_wire=passed fields={len(client[0])} rpcs={len(client[1])}")


if __name__ == "__main__":
    main()
