"""Exercise menu/rejoin through explicit client domain qualification APIs."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import shutil
import tempfile
import time

from capture_watchdog import ProcessTree, run_capture
from case_evidence import record_build


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-bundle", required=True, type=Path)
    parser.add_argument("--server-bundle", type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--port", type=int, default=17561)
    parser.add_argument("--map-dir", type=Path, help="Authored fixture with map.json and main.glb")
    parser.add_argument("--process-priority", choices=("normal", "below-normal"), default="below-normal")
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix="rejoin-", dir=args.evidence_root.resolve()))
    client = args.client_bundle.resolve() / "Octaryn.Client.exe"
    map_dir = args.client_bundle.resolve() / "Client" / "Assets" / "Maps"
    if args.map_dir:
        map_dir = case / "fixture"
        shutil.copytree(args.map_dir.resolve(), map_dir)
        if not (map_dir / "map.json").is_file() or not (map_dir / "main.glb").is_file():
            raise RuntimeError("Rejoin fixture requires map.json and main.glb")
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("OCTARYN_CLIENT_", "OCTARYN_SERVER_"))}
    settings = {"version": 8, "windowWidth": 1280, "windowHeight": 720,
                "fullscreen": False, "renderDistance": 4,
                "upscalerMode": 0, "presentModeIndex": 0, "frameCapFps": 60}
    (case / "settings.json").write_text(json.dumps(settings))
    env.update(OCTARYN_CLIENT_WORLD_PATH=str(case / "client-world"),
               OCTARYN_CLIENT_LIBRARY_ROOT=str(case / "library"),
               OCTARYN_CLIENT_SETTINGS_PATH=str(case / "settings.json"),
               OCTARYN_CLIENT_INVENTORY_PATH=str(case / "inventory.json"),
               OCTARYN_CLIENT_PROFILE_PATH=str(case / "profile.csv"),
               OCTARYN_CLIENT_CAPTURE_PATH=str(case / "frame.bmp"),
               OCTARYN_CLIENT_LIVE_FRAME_TIMING="1",
               OCTARYN_CLIENT_FRAME_TIMING_PATH=str(case / "frame-timing.csv"),
               OCTARYN_CLIENT_GRAPHICS_API="dx12",
               OCTARYN_CLIENT_RHI_VALIDATION="1",
               OCTARYN_CLIENT_UPSCALER="off")
    if args.map_dir:
        env["OCTARYN_CLIENT_MAP_MANIFEST"] = str(map_dir / "map.json")
    env["OCTARYN_CLIENT_LOADING_CAPTURE_DIR"] = str(case / "loading")
    command = [str(client), "--benchmark-hidden", "--validate-session-rejoin"]
    server = None
    server_tree = None
    server_log = Path(__file__).resolve().parents[2] / "logs/server" / (case.name + ".log")
    server_log.parent.mkdir(parents=True, exist_ok=True)
    shutdown = case / "server.shutdown"
    result = {"status": "running", "remote": bool(args.server_bundle),
              "evidence": str(case), "watchdog_frame_ms": 50,
              "watchdog_stall_seconds": 2, "map_dir": str(map_dir),
              "process_priority": args.process_priority}
    record_build(args.client_bundle.resolve(), case)
    print("session_rejoin_started evidence=" + str(case), flush=True)
    try:
        with server_log.open("w") as server_output:
            if args.server_bundle:
                server_env = env.copy()
                server_env["OCTARYN_SERVER_MAP_MODE"] = "1"
                server_env["OCTARYN_SERVER_MAP_PATH"] = str(map_dir / "main.glb")
                server_env["OCTARYN_SERVER_MAP_MANIFEST_PATH"] = str(map_dir / "map.json")
                server_env["OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH"] = str(shutdown)
                server_command = [
                    str(args.server_bundle.resolve() / "Octaryn.Server.exe"),
                    "--listen", f"127.0.0.1:{args.port}",
                    "--world-dir", str(case / "server-world")]
                priority = 0x20 if args.process_priority == "normal" else 0x4000
                flags = (subprocess.CREATE_NO_WINDOW | priority if os.name == "nt" else 0)
                server = subprocess.Popen(server_command,
                    cwd=case, env=server_env, stdout=server_output,
                    stderr=subprocess.STDOUT, creationflags=flags,
                    start_new_session=os.name != "nt")
                server_tree = ProcessTree(server)
                (case / "server-command.json").write_text(json.dumps(server_command, indent=2))
                deadline = time.monotonic() + 45
                while "server_remote_listening" not in server_log.read_text(errors="replace"):
                    if server.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("Dedicated server did not become ready")
                    time.sleep(0.1)
                command += ["--connect", f"127.0.0.1:{args.port}"]
            else:
                command += ["--play-world", "1"]
            (case / "client-command.json").write_text(json.dumps(command, indent=2))
            with (case / "client.log").open("w+b") as output:
                client_exit = run_capture(command, case, env, output, timeout=210,
                                          max_frame_ms=50, process_priority=args.process_priority,
                                          require_clean_exit=True)
            result["client_exit"] = client_exit
            text = (case / "client.log").read_text(errors="replace")
            if client_exit or "session_rejoin=passed sessions=3 menu_returns=2" not in text:
                raise RuntimeError("Three-session menu/rejoin qualification failed")
            starts = [line for line in text.splitlines() if line.startswith("open_world_start ")]
            authority = "remote_server" if args.server_bundle else "bundled_server"
            if len(starts) != 3 or any(f"authority={authority}" not in line for line in starts):
                raise RuntimeError("Menu rejoin changed the selected authority")
            reloads = [line for line in text.splitlines() if line.startswith("world_scene_selected reload=1 source=")]
            if len(reloads) != 2 or reloads[0] != reloads[1]:
                raise RuntimeError("Menu rejoin did not refresh both scenes at the same source path")
            if any(marker in text for marker in ("world_frame_failed", "Validation Error", "D3D12 ERROR")):
                raise RuntimeError("Graphics validation reported an error")
            result.update(sessions=3, menu_returns=2, authority=authority,
                          no_surviving_client_children=True)
            result["status"] = "passed"
    except Exception as error:
        result.update(status="failed", error=str(error))
        raise
    finally:
        if server is not None:
            shutdown.touch()
            try:
                server.wait(timeout=15)
            except subprocess.TimeoutExpired:
                server.terminate()
                server.wait(timeout=10)
            result["server_exit"] = server.returncode
            result["server_log"] = str(server_log)
            if server.returncode != 0 and result["status"] == "passed":
                result.update(status="failed", error="Dedicated server did not shut down cleanly")
            if server_tree is not None:
                try:
                    if server.returncode == 0:
                        with server_log.open("ab") as output:
                            server_tree.verify_clean_exit(output)
                        result["no_surviving_server_children"] = True
                except Exception as error:
                    result.update(status="failed", error=str(error))
                finally:
                    server_tree.close()
        (case / "result.json").write_text(json.dumps(result, indent=2))
    if result["status"] != "passed":
        raise RuntimeError(result["error"])
    print("session_rejoin=passed evidence=" + str(case), flush=True)


if __name__ == "__main__":
    run()
