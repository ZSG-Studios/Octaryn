"""Qualify receipt replacement contention in an isolated production server process."""
import argparse
import ctypes
from ctypes import wintypes
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

from create_fluid_fixture import GENERATION


@contextmanager
def file_handle(path, sharing):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.CreateFileW(str(path), 0x80000000, sharing, None, 3, 0, None)
    if handle == wintypes.HANDLE(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        yield kernel, handle
    finally:
        kernel.CloseHandle(handle)


def read_json(path):
    # Ordinary observation must not introduce another replacement-denying reader.
    with file_handle(path, 7) as (kernel, handle):
        kernel.ReadFile.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                                   ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
        buffer = ctypes.create_string_buffer(1024 * 1024)
        count = wintypes.DWORD()
        if not kernel.ReadFile(handle, buffer, len(buffer), ctypes.byref(count), None):
            raise ctypes.WinError(ctypes.get_last_error())
        return json.loads(buffer.raw[:count.value])


def write_json(path, value):
    temporary = path.with_suffix(".qualification.tmp")
    temporary.write_text(json.dumps(value), encoding="utf-8")
    temporary.replace(path)


def wait_for(process, label, predicate, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"Server exited {process.returncode} while waiting for {label}")
        try:
            value = predicate()
            if value:
                return value
        except (OSError, json.JSONDecodeError):
            pass
        time.sleep(.025)
    raise TimeoutError(label)


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server-bundle", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt":
        raise RuntimeError("This regression requires Windows replacement/share semantics")
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix="receipt-contention-", dir=args.evidence_root.resolve()))
    world, runtime = case / "world", case / "world/runtime"
    runtime.mkdir(parents=True)
    target = dict(x=0, y=162, z=3)
    blocks = [dict(x=x, y=160, z=z, block=5) for x in range(-3, 4) for z in range(0, 9)]
    blocks.append(dict(**target, block=5))
    write_json(world / "world_generation.json", GENERATION)
    write_json(world / "world_blocks.json", dict(version=1, blocks=blocks))
    write_json(world / "player_1.json", dict(version=1, x=.5, y=162.62, z=6,
                                            pitch=0, yaw=0, block=5))
    write_json(runtime / "chunk_view.json", dict(version=1, epoch=1,
               centerChunkX=0, centerChunkZ=0, radius=0, hasPreviousWindow=False))
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("OCTARYN_CLIENT_", "OCTARYN_SERVER_"))}
    paths = dict(WORLD_BLOCKS_PATH=world / "world_blocks.json", PLAYER_SAVE_ROOT=world,
                 CHUNK_VIEW_INTENT_PATH=runtime / "chunk_view.json",
                 CHUNK_STREAM_PATH=runtime / "chunk_stream.json",
                 PLAYER_INPUT_INTENT_PATH=runtime / "player_input.json",
                 PLAYER_STATE_STREAM_PATH=runtime / "player_state.json",
                 BLOCK_INTERACTION_INTENT_PATH=runtime / "block_interaction.json",
                 SHUTDOWN_REQUEST_PATH=runtime / "shutdown.request")
    environment.update({"OCTARYN_SERVER_" + key: str(value) for key, value in paths.items()})
    environment.update(OCTARYN_SERVER_PROCESS_STREAM_LIVE="1",
                       OCTARYN_SERVER_PROCESS_STREAM_INTERVAL_MS="16",
                       OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY="1")
    executable = args.server_bundle.resolve() / "Octaryn.Server.exe"
    log = case / "server.log"
    report = dict(status="running", executable=str(executable), evidence=str(case))
    print("receipt_contention_started evidence=" + str(case), flush=True)
    with log.open("w") as output:
        process = subprocess.Popen([str(executable)], cwd=case, env=environment,
                                   stdout=output, stderr=subprocess.STDOUT)
        report["pid"] = process.pid
        try:
            mailbox = runtime / "block_results.json"
            wait_for(process, "startup receipt mailbox", lambda: mailbox.is_file())
            pose = wait_for(process, "authoritative player pose",
                            lambda: read_json(runtime / "player_state.json"))
            if not 162 < pose["playerY"] < 163:
                raise RuntimeError("Fixture player did not spawn above the authored platform")
            session = read_json(mailbox)["session"]
            with file_handle(mailbox, 1):
                command = dict(requestId=1, editX=target["x"], editY=target["y"], editZ=target["z"],
                               block=0, cameraX=pose["playerX"], cameraY=pose["playerY"],
                               cameraZ=pose["playerZ"], hitX=target["x"], hitY=target["y"], hitZ=target["z"])
                write_json(paths["BLOCK_INTERACTION_INTENT_PATH"],
                           dict(version=1, frameIndex=1, commands=[command]))
                wait_for(process, "deferred publication", lambda:
                         "server_block_receipts publication=deferred" in log.read_text(errors="replace"))
                def persisted_break():
                    data = read_json(world / "world_blocks.json")
                    expected = {(row["x"], row["y"], row["z"], row["block"]) for row in blocks[:-1]}
                    actual = {(row["x"], row["y"], row["z"], row["block"]) for row in data["blocks"]}
                    # Breaking this above-terrain fixture returns to generated air;
                    # edit-only persistence removes that override instead of saving air.
                    return dict(remaining_overrides=len(actual), removed_target=target) if actual == expected else None
                persisted = wait_for(process, "durable authoritative break before receipt", persisted_break)
                before = read_json(runtime / "player_state.json")["sourceTick"]
                wait_for(process, "server continues ticking while mailbox locked", lambda:
                         read_json(runtime / "player_state.json")["sourceTick"] >= before + 30)
                if read_json(mailbox)["receipts"]:
                    raise RuntimeError("Locked mailbox published an acceptance prematurely")
                report.update(durable_while_locked=persisted, locked_tick_start=before,
                              locked_tick_end=read_json(runtime / "player_state.json")["sourceTick"],
                              unpublished_while_locked=True, alive_while_locked=process.poll() is None)
            receipts = wait_for(process, "receipt after unlock", lambda: read_json(mailbox)["receipts"])
            if len(receipts) != 1 or not receipts[0]["accepted"] or receipts[0]["commandID"] != 1:
                raise RuntimeError("Expected exactly one accepted, correlated receipt after unlock")
            report["receipt"] = receipts[0]
            write_json(runtime / "block_results_ack.json",
                       dict(version=1, session=session, sequence=receipts[0]["sequence"]))
            wait_for(process, "ack retirement", lambda: not read_json(mailbox)["receipts"])
            text = log.read_text(errors="replace")
            if "server_block_receipts publication=recovered" not in text:
                raise RuntimeError("Missing publication recovery diagnostic")
            if "edit=break applied=1 changed=1 block=(0,162,3,0)" not in text:
                raise RuntimeError("Missing real authoritative admission/drain evidence")
            report["status"] = "passed"
        except Exception as error:
            report.update(status="failed", error=str(error))
            raise
        finally:
            paths["SHUTDOWN_REQUEST_PATH"].write_text("stop\n")
            try:
                process.wait(timeout=15)
                report["exit_code"] = process.returncode
                if process.returncode != 0 or "octaryn_server_shutdown=1" not in log.read_text(errors="replace"):
                    report.update(status="failed", error="Isolated server did not stop cleanly")
            except subprocess.TimeoutExpired:
                report.update(status="failed", error="Isolated server shutdown request timed out")
            (case / "result.json").write_text(json.dumps(report, indent=2))
    if report["status"] != "passed":
        raise RuntimeError(report["error"])
    print("receipt_contention=passed evidence=" + str(case), flush=True)


if __name__ == "__main__":
    run()
