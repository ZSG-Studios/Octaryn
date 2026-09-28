"""Run real ModuleActivator/ItemSystem/Box3D ownership without renderer overhead."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

from make_tile_fixture import payload

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, choices=(100, 1000, 10000), required=True)
    args = parser.parse_args()
    evidence = Path(tempfile.mkdtemp(prefix="authority-items-", dir=ROOT / "logs/server"))
    build = ROOT / "build/release-windows"
    world = evidence / "world"
    world.mkdir()
    (world / "main.glb").write_bytes(payload(0, [.5,.5,.5,1]))
    (world / "map.json").write_text(json.dumps(dict(version=1, map="main.glb", spawn=[0,3,-9], yaw=0., pitch=0.)))
    environment = {k:v for k,v in os.environ.items() if not k.startswith("OCTARYN_")}
    environment.update({"OCTARYN_SERVER_MAP_MODE":"1", "OCTARYN_SERVER_MAP_PATH":str(world / "main.glb"),
        "OCTARYN_SERVER_MAP_MANIFEST_PATH":str(world / "map.json"), "OCTARYN_SERVER_WORLD_DIR":str(world),
        "OCTARYN_SERVER_PLAYER_SAVE_ROOT":str(world), "OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY":"1"})
    command = ["dotnet", str(build / "tools/SessionTransportProbe/managed/SessionTransportProbe.dll"),
               "--authority-items", str(build / "server/bundle"), str(args.count)]
    with (evidence / "run.log").open("wb") as output:
        subprocess.run(command, cwd=ROOT, env=environment, stdout=output, stderr=subprocess.STDOUT, check=True, timeout=60)
    print("authority_item_probe=passed evidence=" + str(evidence))


if __name__ == "__main__":
    main()
