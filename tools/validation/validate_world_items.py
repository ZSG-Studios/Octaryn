"""Qualify actual packaged toss/render/pickup through domain APIs, without OS input."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

from validate_rhi_client_diagnostic import inspect_result


UPSCALERS = ("off", "native", "quality", "balanced", "performance", "ultra-performance")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def inspect_items(returncode, text, case, bundle, backend, upscaler, expected_count=1):
    frames, columns, quads = inspect_result(
        returncode, text, api="D3D12" if backend == "dx12" else "Vulkan", minimum_frames=0)
    accepted = re.search(
        r"world_items_validation phase=accepted item=(\d+) block=(\d+) count=(\d+) "
        r"inventory_after_drop=(\d+) server_seconds=([\d.]+)", text)
    completed = re.search(
        r"world_items_validation=passed item=(\d+) block=(\d+) count=(\d+) "
        r"inventory_before=(\d+) inventory_after=(\d+) grant=(\d+) server_ack=(\d+) "
        r"capture=completed os_events_injected=0", text)
    captured = re.search(r"world_items_validation phase=captured item=(\d+) position=([^\r\n]+) count=(\d+)", text)
    capture_log = re.compile(r"world_capture frame=(\d+) columns=(\d+) nonclear_pixels=(\d+)").search(
        text, accepted.end() if accepted else 0)
    require(accepted and completed and captured and capture_log,
            "Missing accepted toss, presented capture, or acknowledged pickup evidence")
    item, block, count, before, after, grant, ack = map(int, completed.groups())
    accepted_item, accepted_block, accepted_count, debited = map(int, accepted.groups()[:4])
    require(item == accepted_item == int(captured.group(1)) and block == accepted_block,
            "Toss, capture and pickup refer to different authoritative items")
    require(count == accepted_count == int(captured.group(3)) == expected_count and before == after == debited + count,
            "Inventory debit/credit or stack count is inconsistent")
    entities = (count + 63) // 64
    split = re.search(r"world_items_validation phase=split entities=(\d+) total=(\d+) full_stacks=(\d+) "
                      r"remainder=(\d+) first_item=(\d+) last_item=(\d+)", text)
    require(split and tuple(map(int, split.groups())) ==
            (entities, count, count // 64, count % 64, item, item + entities - 1),
            "Authoritative snapshot did not prove the complete bounded entity split")
    credits = [tuple(map(int, match.groups())) for match in re.finditer(
        r"world_items_validation phase=pickup_credit grant=(\d+) block=(\d+) count=(\d+)", text)]
    require(len(credits) == entities and [row[0] for row in credits] == list(range(1, entities+1))
            and all(row[1] == block and 1 <= row[2] <= 64 for row in credits)
            and sum(row[2] for row in credits) == count and grant == entities,
            "Saved inventory credits do not prove ordered pickup count conservation")
    require(grant > 0 and ack == grant, "Pickup was not acknowledged by the server")
    require(accepted.start() < capture_log.start() < captured.start() < completed.start(),
            "Capture did not occur between accepted toss and completed pickup")
    require(int(capture_log.group(2)) == columns and int(capture_log.group(3)) > 0,
            "Captured frame omitted the complete world or contains no rendered pixels")
    require(not re.search(r"rmlui severity=(?:error|warning)|rml_ui\w*=failed|world_items_\w*=failed", text),
            "UI/item validation reported a warning or error")
    mode = UPSCALERS.index(upscaler)
    fsr = re.findall(r"world_fsr2 version=2\.2\.1 mode=(\d+) render=(\d+)x(\d+) output=(\d+)x(\d+)", text)
    require((not fsr) if mode == 0 else bool(fsr) and int(fsr[-1][0]) == mode,
            "Actual temporal upscaler does not match the requested mode")

    capture = case / "frame.bmp"
    image = capture.read_bytes()
    require(len(image) > 1024 and image[:2] == b"BM", "GPU BMP capture is missing or invalid")
    width, height = struct.unpack_from("<ii", image, 18)
    require(width >= 64 and abs(height) >= 64, "GPU capture has invalid dimensions")

    # Read the actual final server save, independently of the client's pass marker.
    saved = (case / "world/world_items.bin").read_bytes()
    require(len(saved) == 15440 + 32 and hashlib.sha256(saved[32:]).digest() == saved[:32],
            "Final server item save checksum/schema is invalid")
    state = struct.unpack_from("<IIQQQQddIIIIII", saved, 32)
    require(state[0:2] == (1, 15440) and state[5] == ack and state[11:13] == (0, 0),
            "Final server save retains the item/grant or lacks its committed acknowledgement")
    require(state[2:5] == (entities+1, entities+1, 1) and state[8:11] == (block, count, 1),
            "Saved server command/split/grant counters do not match the single accepted toss")
    inventory = json.loads((case / "inventory.json").read_text(encoding="utf-8"))
    require(inventory.get("grant_watermark") == grant and inventory.get("drop_watermark") == state[4]
            and inventory.get("reserved_drop") == 0,
            "Final inventory did not persist both receipt watermarks and release its reservation")
    catalog = json.loads((bundle / "Data/Blocks/octaryn.basegame.blocks.json").read_text(encoding="utf-8"))
    require(block < len(catalog["blocks"]), "Server block ID is absent from the bundled catalog")
    key = catalog["blocks"][block]["id"]
    total = sum(amount for slot, amount in zip(inventory["slots"], inventory["counts"]) if slot == key)
    if inventory.get("cursor") == key:
        total += inventory.get("cursor_count", 0)
    require(total == after, "Saved inventory quantity differs from the live completion counters")
    server_log = case / "world/logs/server/local-session.log"
    require(server_log.is_file() and "octaryn_server_shutdown=1" in server_log.read_text(encoding="utf-8", errors="replace"),
            "Isolated authoritative server did not report its graceful shutdown")
    return {"frames": frames, "columns": columns, "quads": quads, "item": item, "block": block,
            "count": count, "split_entities": entities, "pickup_credits": credits,
            "inventory_before": before, "inventory_after": after, "grant": grant,
            "server_ack": ack, "capture": str(capture), "width": width, "height": abs(height)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-bundle-root", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--backend", required=True, choices=("dx12", "vulkan"))
    parser.add_argument("--upscaler", choices=UPSCALERS, default="off")
    parser.add_argument("--whole-stack", action="store_true",
                        help="Seed one isolated 999-item inventory stack and verify its real 16-entity toss/pickup")
    parser.add_argument("--provisional", action="store_true",
                        help="Capture a provisional toss before releasing its authoritative intent")
    args = parser.parse_args()
    bundle = args.client_bundle_root.resolve()
    executable = bundle / ("Octaryn.Client.exe" if os.name == "nt" else "Octaryn.Client")
    require(executable.is_file(), f"Packaged client missing: {executable}")
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix="world-items-", dir=args.evidence_root.resolve()))
    (case / "world").mkdir()
    settings = {"version": 8, "windowWidth": 1280, "windowHeight": 720, "fullscreen": False,
                "renderDistance": 4, "upscalerMode": UPSCALERS.index(args.upscaler), "presentModeIndex": 0}
    (case / "settings.json").write_text(json.dumps(settings), encoding="utf-8")
    expected_count = 999 if args.whole_stack else 1
    if args.whole_stack:
        catalog = json.loads((bundle / "Data/Blocks/octaryn.basegame.blocks.json").read_text(encoding="utf-8"))
        key = "octaryn.basegame.block.grass"
        require(any(block.get("id") == key and block.get("placeable") for block in catalog["blocks"]),
                "Bundled catalog lacks the placeable grass used by this inventory-only fixture")
        palette = {"schema": "octaryn.client.build-palette.v2", "selected": 0,
                   "slots": [key] + [""] * 49, "counts": [999] + [0] * 49,
                   "cursor": "", "cursor_count": 0, "grant_watermark": 0,
                   "drop_watermark": 0, "reserved_drop": 0}
        (case / "inventory.json").write_text(json.dumps(palette), encoding="utf-8")
        (case / "initial-inventory.json").write_text(json.dumps(palette), encoding="utf-8")
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("OCTARYN_CLIENT_", "OCTARYN_SERVER_"))}
    overrides = {
        "OCTARYN_CLIENT_WORLD_PATH": str(case / "world"),
        "OCTARYN_CLIENT_SETTINGS_PATH": str(case / "settings.json"),
        "OCTARYN_CLIENT_LIGHTING_PATH": str(case / "lighting.json"),
        "OCTARYN_CLIENT_INVENTORY_PATH": str(case / "inventory.json"),
        "OCTARYN_CLIENT_CAPTURE_PATH": str(case / "frame.bmp"),
        "OCTARYN_CLIENT_PROFILE_PATH": str(case / "world-profile.csv"),
        "OCTARYN_CLIENT_RHI_VALIDATION": "1",
        "OCTARYN_CLIENT_GRAPHICS_API": args.backend,
        "OCTARYN_CLIENT_UPSCALER": args.upscaler,
        "OCTARYN_CLIENT_VALIDATE_ITEM_COUNT": str(expected_count),
    }
    if args.provisional:
        overrides["OCTARYN_CLIENT_VALIDATE_PROVISIONAL_TOSS"] = "1"
    if args.backend == "vulkan":
        overrides["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
        if os.name == "nt":
            layers = Path(__file__).resolve().parents[2] / "build/dependencies/vulkan-validation"
            require((layers / "VkLayer_khronos_validation.json").is_file() and
                    (layers / "vk_layer_settings.txt").is_file(), "Pinned Vulkan validation layer/settings are missing")
            overrides.update(VK_LAYER_PATH=str(layers), VK_LAYER_SETTINGS_PATH=str(layers))
    environment.update(overrides)
    command = [str(executable), "--validate-world-items", "--benchmark-hidden"]
    report = {"status": "running", "backend": args.backend, "upscaler": args.upscaler,
              "requested_count": expected_count, "fixture": "inventory_only" if args.whole_stack else "default_inventory",
              "command": command, "environment_overrides": overrides,
              "client_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(), "evidence": str(case)}
    report_path = case / "result.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"world_items_validation_started evidence={case}", flush=True)
    try:
        with (case / "client.log").open("wb") as output:
            process = subprocess.Popen(command, cwd=case, env=environment,
                                       stdout=output, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=110)
            except subprocess.TimeoutExpired:
                runtime = case / "world/runtime"
                runtime.mkdir(exist_ok=True)
                (runtime / "shutdown.request").write_text("stop\n", encoding="utf-8")
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
                raise RuntimeError("World item validation exceeded110 seconds")
        text = (case / "client.log").read_text(encoding="utf-8", errors="replace")
        report.update(inspect_items(code, text, case, bundle, args.backend, args.upscaler, expected_count))
        if args.provisional:
            for phase in ("provisional_captured", "provisional_reconciled", "authoritative_captured"):
                require("phase=" + phase in text, "Missing toss phase: " + phase)
            provisional = case / "frame.bmp.provisional.bmp"
            require(provisional.is_file(), "Missing provisional GPU capture")
            report["provisional_capture"] = str(provisional)
        report["status"] = "passed"
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"world_items_packaged=passed backend={args.backend} upscaler={args.upscaler} evidence={case}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(f"world_items_packaged=failed: {error}", file=sys.stderr)
        sys.exit(1)
