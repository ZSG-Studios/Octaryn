"""Isolated server mailbox sessions and exact item event checks for probes."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]


class ItemProbe:
    def __init__(self, name):
        logs = ROOT / "logs" / "server"
        logs.mkdir(parents=True, exist_ok=True)
        self.directory = Path(tempfile.mkdtemp(prefix=name + "-", dir=logs))
        self.runtime = self.directory / "runtime"
        self.runtime.mkdir()
        self.log_path = self.directory / "server.log"
        self.server = None
        self.log = None

    def __enter__(self):
        preset = os.environ.get("OCTARYN_PROBE_PRESET", "release-windows")
        bundle = ROOT / "build" / preset / "server" / "bundle"
        maps = ROOT / "build" / preset / "client" / "bundle" / "Client" / "Assets" / "Maps"
        self.write("chunk_view.json", {
            "version": 1, "epoch": 1, "centerChunkX": 0, "centerChunkZ": 0,
            "radius": 4, "hasPreviousWindow": False,
            "previousCenterChunkX": 0, "previousCenterChunkZ": 0, "previousRadius": 4})
        env = os.environ.copy()
        env.update({
            "OCTARYN_SERVER_PROCESS_STREAM_LIVE": "1",
            "OCTARYN_SERVER_PROCESS_STREAM_INTERVAL_MS": "16",
            "OCTARYN_CLIENT_DISABLE_GAME_MODULES": "0",
            "OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY": "0",
            "OCTARYN_SERVER_PLAYER_SAVE_ROOT": str(self.directory),
            "OCTARYN_SERVER_MAP_MODE": "1",
            "OCTARYN_SERVER_MAP_PATH": str(maps / "main.glb"),
            "OCTARYN_SERVER_MAP_MANIFEST_PATH": str(maps / "map.json"),
        })
        for variable, filename in {
            "CHUNK_VIEW_INTENT_PATH": "chunk_view.json",
            "PLAYER_INPUT_INTENT_PATH": "player_input.json",
            "UI_ACTION_INTENT_PATH": "ui_action.json",
            "PLAYER_STATE_STREAM_PATH": "player_state.json",
            "SHUTDOWN_REQUEST_PATH": "shutdown.request",
            "WORLD_TIME_INTENT_PATH": "world_time.json",
        }.items():
            env["OCTARYN_SERVER_" + variable] = str(self.runtime / filename)
        executable = "Octaryn.Server.exe" if os.name == "nt" else "Octaryn.Server"
        self.log = self.log_path.open("w", encoding="utf-8")
        try:
            self.server = subprocess.Popen(
                [str(bundle / executable)], cwd=self.directory, env=env,
                stdout=self.log, stderr=subprocess.STDOUT)
        except BaseException:
            self.log.close()
            raise
        print(f"PROBE log={self.log_path}")
        return self

    def __exit__(self, *_):
        try:
            (self.runtime / "shutdown.request").write_text("stop\n", encoding="utf-8")
            try:
                self.server.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait(timeout=10)
        finally:
            self.log.close()

    def write(self, name, payload):
        temporary = self.runtime / (name + ".tmp")
        for attempt in range(20):
            try:
                temporary.write_text(json.dumps(payload), encoding="utf-8")
                os.replace(temporary, self.runtime / name)
                return
            except PermissionError:
                if attempt == 19:
                    raise
                time.sleep(0.002)

    def pose(self):
        try:
            return json.loads((self.runtime / "player_state.json").read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None

    def wait_pose(self):
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            pose = self.pose()
            if pose:
                return pose
            if self.server.poll() is not None:
                raise RuntimeError(f"server exited before pose: {self.server.returncode}")
            time.sleep(0.25)
        raise RuntimeError("no_pose_stream")

    def text(self):
        return self.log_path.read_text(encoding="utf-8", errors="replace")

    def input(self, frame, pitch, yaw, ack, forward=0.0):
        commands = [{"frameIndex": index, "flags": 0, "controller": 1,
                     "moveX": 0.0, "moveY": 0.0, "moveZ": forward,
                     "cameraPitch": pitch, "cameraYaw": yaw, "relativeMouse": 1}
                    for index in range(ack + 1, min(frame + 1, ack + 65))]
        self.write("player_input.json", {"version": 2, "commands": commands})


def check_single_pickup(text):
    drops = [tuple(map(int, match)) for match in re.findall(
        r"\bitem_drop id=(\d+) count=(\d+)\b", text)]
    pickups = [tuple(map(int, match)) for match in re.findall(
        r"\bitem_pickup id=(\d+) granted=(\d+) remaining=(\d+)\b", text)]
    if drops != [(2, 1)] or pickups != [(2, 1, 0)]:
        raise RuntimeError(f"unexpected item event counts: drops={drops} pickups={pickups}")

