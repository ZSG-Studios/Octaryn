"""Check actual authority saved-pose initialization and saved-location collision readiness."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

from make_tile_fixture import payload

ROOT = Path(__file__).resolve().parents[2]


def main():
    evidence = Path(tempfile.mkdtemp(prefix="player-spawn-", dir=ROOT / "logs/server"))
    build = ROOT / "build/release-windows"
    for mode in ("saved", "fresh", "diagnostic"):
        world = evidence / mode
        world.mkdir()
        for index in range(5):
            (world / f"tile-{index}.glb").write_bytes(payload(index * 24, [.5, .5, .5, 1]))
        manifest = dict(version=1, map="tile-0.glb", spawn=[0, 3, -9], yaw=.6, pitch=-.15,
                        tiles=[[i*24-12, -1, -12, i*24+12, 6, 12] for i in range(5)],
                        tile_files=[f"tile-{i}.glb" for i in range(5)])
        (world / "map.json").write_text(json.dumps(manifest))
        if mode != "fresh":
            (world / "player_1.json").write_text(json.dumps(
                dict(version=1, x=96, y=3, z=-3, pitch=-.1, yaw=1.2)))
        env = {k: v for k, v in os.environ.items() if not k.startswith("OCTARYN_")}
        env.update(OCTARYN_SERVER_MAP_MODE="1", OCTARYN_SERVER_MAP_PATH=str(world / "tile-0.glb"),
                   OCTARYN_SERVER_MAP_MANIFEST_PATH=str(world / "map.json"),
                   OCTARYN_SERVER_WORLD_DIR=str(world), OCTARYN_SERVER_PLAYER_SAVE_ROOT=str(world),
                   OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH=str(world / "authority.log"))
        if mode == "diagnostic":
            env["OCTARYN_SERVER_DIAGNOSTIC_MAP_SPAWN"] = "1"
        command = ["dotnet", str(build / "tools/SessionTransportProbe/managed/SessionTransportProbe.dll"),
                   "--player-spawn", str(build / "server/bundle"), str(world), mode]
        with (world / "run.log").open("w") as log:
            subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
                           check=True, timeout=30)
    print("player_spawn_probe=passed evidence=" + str(evidence))


if __name__ == "__main__":
    main()
