"""Restart the production module authority in distinct processes and save roots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def floor_fixture(path):
    vertices = [(-20, 0, -20), (20, 0, -20), (20, 0, 20), (-20, 0, 20)]
    data = struct.pack("<12f6I", *[v for row in vertices for v in row], 0, 2, 1, 0, 3, 2)
    document = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
                "nodes": [{"mesh": 0}], "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
                "buffers": [{"byteLength": len(data)}],
                "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 48},
                                {"buffer": 0, "byteOffset": 48, "byteLength": 24}],
                "accessors": [{"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
                               "min": [-20, 0, -20], "max": [20, 0, 20]},
                              {"bufferView": 1, "componentType": 5125, "count": 6, "type": "SCALAR"}]}
    text = json.dumps(document).encode(); text += b" " * (-len(text) % 4)
    payload = struct.pack("<III", 0x46546C67, 2, 28 + len(text) + len(data))
    payload += struct.pack("<II", len(text), 0x4E4F534A) + text
    payload += struct.pack("<II", len(data), 0x004E4942) + data
    path.write_bytes(payload)


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-bundle", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--preset", default="world-save-check")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix="world-saves-", dir=args.evidence_root.resolve()))
    project = repo / "tools/validation/WorldSaveProbe/WorldSaveProbe.csproj"
    subprocess.run(["dotnet", "build", str(project), "-c", "Release", "--nologo",
                    "-p:OctarynBuildPresetName=" + args.preset], cwd=repo, check=True)
    probe = repo / "build" / args.preset / "tools/WorldSaveProbe/managed/Octaryn.ServerPersistenceProbe.dll"
    floor_fixture(case / "floor.glb")
    (case / "map.json").write_text(json.dumps({"version": 1, "map": "floor.glb", "spawn": [0, 1.62, 0], "yaw": 0, "pitch": 0}))
    env = {k: v for k, v in os.environ.items() if not k.startswith(("OCTARYN_SERVER_", "OCTARYN_CLIENT_"))}
    env.update(OCTARYN_SERVER_MAP_MODE="1", OCTARYN_SERVER_MAP_PATH=str(case / "floor.glb"),
               OCTARYN_SERVER_MAP_MANIFEST_PATH=str(case / "map.json"), OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY="1")
    native = args.server_bundle.resolve()
    for key, filename in {
        "OCTARYN_NATIVE_JOBS_LIBRARY": "octaryn_native_jobs",
        "OCTARYN_SERVER_HOST_LIBRARY": "octaryn_server_host",
        "OCTARYN_SERVER_MAP_WORLD_LIBRARY": "octaryn_server_map_world",
        "OCTARYN_SERVER_PLAYER_SIMULATION_LIBRARY": "octaryn_server_player_simulation",
        "OCTARYN_SERVER_WORLD_TIME_LIBRARY": "octaryn_server_world_time",
        "OCTARYN_SERVER_AUTHORITY_TICK_LIBRARY": "octaryn_server_authority_tick",
        "OCTARYN_SERVER_SESSION_STREAM_LIBRARY": "octaryn_server_session_stream",
        "OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY": "octaryn_server_world_persistence",
    }.items(): env[key] = str(native / (filename + ".dll"))
    results = []
    def invoke(mode, variant, expected_success=True):
        result = subprocess.run(["dotnet", str(probe), mode, str(case / ("world-" + variant)), variant],
                                cwd=repo, env=env, capture_output=True, text=True, timeout=60)
        (case / (mode + "-" + variant + ".log")).write_text(result.stdout + result.stderr)
        if expected_success != (result.returncode == 0):
            raise RuntimeError(f"{mode}/{variant} failed: {result.stdout[-2000:]} {result.stderr[-2000:]}")
        results.append({"mode": mode, "world": variant, "exit": result.returncode})
    for variant in ("a", "b"): invoke("seed", variant)
    for variant in ("a", "b"): invoke("verify", variant)
    invoke("scale-seed", "scale")
    invoke("scale-verify", "scale")
    invoke("queue", "queue")
    invoke("receipt-crash", "crash")
    invoke("receipt-recover", "crash")
    save = case / "world-a/world-state.save"
    original = save.read_bytes(); broken = bytearray(original); broken[-1] ^= 1; save.write_bytes(broken)
    digest = hashlib.sha256(broken).hexdigest()
    invoke("load", "a", False)
    assert hashlib.sha256(save.read_bytes()).hexdigest() == digest, "Corrupt save was overwritten"
    save.write_bytes(original)
    (case / "results.json").write_text(json.dumps({"status": "passed", "cases": results,
        "checks": ["separate_process_restart", "two_world_isolation", "pose", "inventory_no_regrant",
                   "world_items_counts_ids", "entity_watermark", "clock_native_blob", "receipt_namespace", "corruption_preserved",
                   "invalid_snapshot_no_mutation", "10000_item_snapshot_bound", "ordered_save_queue_failures",
                   "live_autosave", "receipt_namespace_reserved_before_crash"]}, indent=2))
    print("world_saves=passed evidence=" + str(case))


if __name__ == "__main__": run()
