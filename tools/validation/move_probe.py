"""Feed movement commands to a live local server and watch the pose stream."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[2]
runtime = root / "logs" / "tools" / "moveprobe" / "runtime"
runtime.mkdir(parents=True, exist_ok=True)
for stale in ("player_input.json", "player_state.json", "shutdown.request", "world_time.json"):
    (runtime / stale).unlink(missing_ok=True)
(runtime / "chunk_view.json").write_text(json.dumps({
    "version": 1, "epoch": 1, "centerChunkX": 0, "centerChunkZ": 0, "radius": 4,
    "hasPreviousWindow": False, "previousCenterChunkX": 0,
    "previousCenterChunkZ": 0, "previousRadius": 4}))

bundle = root / "build" / "release-windows" / "server" / "bundle"
maps = root / "build" / "release-windows" / "client" / "bundle" / "Client" / "Assets" / "Maps"
env = os.environ.copy()
env.update({
    "OCTARYN_SERVER_PROCESS_STREAM_LIVE": "1",
    "OCTARYN_SERVER_PROCESS_STREAM_INTERVAL_MS": "16",
    "OCTARYN_CLIENT_DISABLE_GAME_MODULES": "0",
    "OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY": "0",
    "OCTARYN_SERVER_WORLD_BLOCKS_PATH": str(runtime / "world_blocks.json"),
    "OCTARYN_SERVER_PLAYER_SAVE_ROOT": str(runtime.parent),
    "OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH": str(runtime / "chunk_view.json"),
    "OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH": str(runtime / "player_input.json"),
    "OCTARYN_SERVER_PLAYER_STATE_STREAM_PATH": str(runtime / "player_state.json"),
    "OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH": str(runtime / "shutdown.request"),
    "OCTARYN_SERVER_WORLD_TIME_INTENT_PATH": str(runtime / "world_time.json"),
    "OCTARYN_SERVER_MAP_MODE": "1",
    "OCTARYN_SERVER_MAP_PATH": str(maps / "main.glb"),
    "OCTARYN_SERVER_MAP_MANIFEST_PATH": str(maps / "map.json"),
})
log = (runtime.parent / "server.log").open("w")
server = subprocess.Popen([str(bundle / "Octaryn.Server.exe")],
                          cwd=runtime.parent, env=env, stdout=log, stderr=subprocess.STDOUT)

def read_pose():
    try:
        return json.loads((runtime / "player_state.json").read_text())
    except Exception:
        return None

try:
    deadline = time.monotonic() + 60
    pose = None
    while time.monotonic() < deadline:
        pose = read_pose()
        if pose:
            break
        time.sleep(0.25)
    if not pose:
        print("PROBE=failed reason=no_pose_stream")
        sys.exit(1)
    spawn = (pose["playerX"], pose["playerY"], pose["playerZ"])
    print(f"PROBE spawn={spawn} tick={pose['sourceTick']}")

    # Stream 6 seconds of walk-forward commands at 60 Hz, resending all
    # unacknowledged commands per packet like the real client transport.
    frame = 0
    ack = 0
    start = time.monotonic()
    while time.monotonic() - start < 6.0:
        frame += 1
        pose = read_pose()
        if pose:
            ack = max(ack, pose.get("acknowledgedInputFrame", 0))
        commands = [{"frameIndex": index, "flags": 0, "controller": 1,
                     "moveX": 0.0, "moveY": 0.0, "moveZ": 1.0,
                     "cameraPitch": -0.25, "cameraYaw": 0.6, "relativeMouse": 1}
                    for index in range(ack + 1, frame + 1)][:64]
        payload = json.dumps({"version": 2, "commands": commands})
        for _ in range(20):
            try:
                tmp = runtime / "player_input.json.tmp"
                tmp.write_text(payload)
                os.replace(tmp, runtime / "player_input.json")
                break
            except PermissionError:
                time.sleep(0.002)
        time.sleep(1.0 / 60.0)

    time.sleep(0.5)
    pose = read_pose()
    end = (pose["playerX"], pose["playerY"], pose["playerZ"])
    moved = sum((a - b) ** 2 for a, b in zip(end, spawn)) ** 0.5
    print(f"PROBE end={end} ack={pose['acknowledgedInputFrame']} moved={moved:.3f}")
    print("PROBE=" + ("passed" if moved > 3.0 and pose["acknowledgedInputFrame"] > 100 else "failed"))
finally:
    (runtime / "shutdown.request").write_text("stop\n")
    try:
        server.wait(timeout=10)
    except subprocess.TimeoutExpired:
        server.kill()
    log.close()
