"""Exercise menu/rejoin through explicit client domain qualification APIs."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-bundle", required=True, type=Path)
    parser.add_argument("--server-bundle", type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--port", type=int, default=17561)
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix="rejoin-", dir=args.evidence_root.resolve()))
    client = args.client_bundle.resolve() / "Octaryn.Client.exe"
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("OCTARYN_CLIENT_", "OCTARYN_SERVER_"))}
    settings = {"version": 8, "windowWidth": 1280, "windowHeight": 720,
                "fullscreen": False, "renderDistance": 4,
                "upscalerMode": 0, "presentModeIndex": 0}
    (case / "settings.json").write_text(json.dumps(settings))
    env.update(OCTARYN_CLIENT_WORLD_PATH=str(case / "client-world"),
               OCTARYN_CLIENT_SETTINGS_PATH=str(case / "settings.json"),
               OCTARYN_CLIENT_INVENTORY_PATH=str(case / "inventory.json"),
               OCTARYN_CLIENT_PROFILE_PATH=str(case / "profile.csv"),
               OCTARYN_CLIENT_CAPTURE_PATH=str(case / "frame.bmp"),
               OCTARYN_CLIENT_GRAPHICS_API="dx12",
               OCTARYN_CLIENT_RHI_VALIDATION="1",
               OCTARYN_CLIENT_UPSCALER="off")
    command = [str(client), "--validate-session-rejoin"]
    server = None
    server_log = Path(__file__).resolve().parents[2] / "logs/server" / (case.name + ".log")
    server_log.parent.mkdir(parents=True, exist_ok=True)
    shutdown = case / "server.shutdown"
    result = {"status": "running", "remote": bool(args.server_bundle),
              "evidence": str(case)}
    print("session_rejoin_started evidence=" + str(case), flush=True)
    try:
        with server_log.open("w") as server_output:
            if args.server_bundle:
                server_env = env.copy()
                server_env["OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH"] = str(shutdown)
                server = subprocess.Popen([
                    str(args.server_bundle.resolve() / "Octaryn.Server.exe"),
                    "--listen", f"127.0.0.1:{args.port}",
                    "--world-dir", str(case / "server-world")],
                    cwd=case, env=server_env, stdout=server_output,
                    stderr=subprocess.STDOUT)
                deadline = time.monotonic() + 45
                while "server_remote_listening" not in server_log.read_text(errors="replace"):
                    if server.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("Dedicated server did not become ready")
                    time.sleep(0.1)
                command += ["--connect", f"127.0.0.1:{args.port}"]
            else:
                command += ["--play-world", "1"]
            with (case / "client.log").open("w") as output:
                completed = subprocess.run(command, cwd=case, env=env, stdout=output,
                                           stderr=subprocess.STDOUT, timeout=210)
            text = (case / "client.log").read_text(errors="replace")
            if completed.returncode or "session_rejoin=passed sessions=3 menu_returns=2" not in text:
                raise RuntimeError("Three-session menu/rejoin qualification failed")
            if any(marker in text for marker in ("world_frame_failed", "Validation Error", "D3D12 ERROR")):
                raise RuntimeError("Graphics validation reported an error")
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
        (case / "result.json").write_text(json.dumps(result, indent=2))
    if result["status"] != "passed":
        raise RuntimeError(result["error"])
    print("session_rejoin=passed evidence=" + str(case), flush=True)


if __name__ == "__main__":
    run()
