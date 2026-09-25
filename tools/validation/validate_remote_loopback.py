#!/usr/bin/env python3
"""Verify the packaged dedicated server and a remote client session.

Starts Octaryn.Server with --listen, waits for its ready signal, then runs
the remote loopback probe (handshake, authoritative pose, chunk snapshot,
block edit acknowledgement and reconnect) against the native-callable client
remote transport. Finally checks the server log for the matching session
events.
"""
import argparse
import os
import pathlib
import queue
import shutil
import subprocess
import sys
import threading


def pump_lines(stream, lines):
    for line in stream:
        lines.put(line.rstrip("\n"))
    stream.close()


def wait_for_line(lines, needle, timeout_seconds, collected):
    import time
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        try:
            line = lines.get(timeout=1)
        except queue.Empty:
            continue
        collected.append(line)
        print(f"[server] {line}")
        if needle in line:
            return True
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-bundle", required=True)
    parser.add_argument("--client-bundle", required=True)
    parser.add_argument("--work-root", required=True)
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--port", type=int, default=17531)
    parser.add_argument("--timeout-seconds", type=int, default=240)
    parser.add_argument("--configuration", default="Release")
    args = parser.parse_args()

    server_bundle = pathlib.Path(args.server_bundle).resolve()
    client_bundle = pathlib.Path(args.client_bundle).resolve()
    repo_root = pathlib.Path(args.repo_root).resolve()
    work = pathlib.Path(args.work_root).resolve()
    world = work / "world"
    runtime = work / "client-runtime"

    server_exe = server_bundle / ("Octaryn.Server.exe" if os.name == "nt" else "Octaryn.Server")
    bridge_name = "octaryn_client_managed_bridge.dll" if os.name == "nt" else "liboctaryn_client_managed_bridge.so"
    bridge = client_bundle / bridge_name
    errors = []
    if not server_exe.is_file():
        errors.append(f"{server_exe}: dedicated server entrypoint is missing")
    if not bridge.is_file():
        errors.append(f"{bridge}: client remote bridge library is missing")
    if errors:
        for error in errors:
            print(f"remote loopback: {error}", file=sys.stderr)
        return 1

    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)

    endpoint = f"127.0.0.1:{args.port}"
    server_lines: queue.Queue[str] = queue.Queue()
    collected: list[str] = []
    server = subprocess.Popen(
        [str(server_exe), "--listen", endpoint, "--world-dir", str(world)],
        cwd=str(server_bundle),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    pump = threading.Thread(
        target=pump_lines, args=(server.stdout, server_lines), daemon=True)
    pump.start()
    try:
        if not wait_for_line(server_lines, "octaryn_server_ready=1",
                             args.timeout_seconds, collected):
            raise RuntimeError("dedicated server never reported ready")
        print(f"remote loopback: dedicated server ready at {endpoint}")

        probe = subprocess.run(
            ["dotnet", "run", "--project",
             str(repo_root / "tools/validation/Octaryn.RemoteLoopbackProbe"
                 / "Octaryn.RemoteLoopbackProbe.csproj"),
             "--configuration", args.configuration, "--",
             "--bridge", str(bridge), "--endpoint", endpoint,
             "--runtime", str(runtime), "--timeout-seconds", "120"],
            cwd=str(repo_root),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=args.timeout_seconds,
        )
        print(probe.stdout)
        if probe.returncode != 0 or "remote_loopback=passed" not in probe.stdout:
            raise RuntimeError(
                f"loopback probe failed with exit {probe.returncode}")
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"remote loopback: {error}", file=sys.stderr)
        server.terminate()
        try:
            server.wait(timeout=15)
        except subprocess.TimeoutExpired:
            server.kill()
        drain(server_lines, collected)
        return 1

    server.terminate()
    try:
        server.wait(timeout=30)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait(timeout=30)
    drain(server_lines, collected)

    for needle in ("server_remote_listening",
                   "server_remote_hello accepted=1",
                   "server_remote_block_ack"):
        if not any(needle in line for line in collected):
            print(f"remote loopback: server log is missing {needle}",
                  file=sys.stderr)
            return 1
    print("remote_loopback=passed "
          f"endpoint={endpoint} world={world}")
    return 0


def drain(lines, collected):
    while True:
        try:
            line = lines.get_nowait()
        except queue.Empty:
            return
        collected.append(line)
        print(f"[server] {line}")


if __name__ == "__main__":
    raise SystemExit(main())
