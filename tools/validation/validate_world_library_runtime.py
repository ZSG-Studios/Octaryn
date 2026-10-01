"""Qualify the packaged world menu and independent gameplay saves without OS input."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile

from capture_watchdog import run_capture
from case_evidence import record_build
from validate_world_library import fixtures
from world_library_evidence import lazy_menu, library_surface, selected_loading

ROOT = Path(__file__).resolve().parents[2]


def run(bundle, folder, arguments, library, size=(1280, 720), world=None, expected_exit=0,
        extra_environment=None, require_clean_exit=False):
    folder.mkdir(parents=True, exist_ok=True)
    print(f"world_library_case status=running name={folder.name} evidence={folder}", flush=True)
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("OCTARYN_CLIENT_", "OCTARYN_SERVER_"))}
    settings = folder / "settings.json"
    settings.write_text(json.dumps(dict(windowWidth=size[0], windowHeight=size[1],
                                       fullscreen=False, renderDistance=4,
                                       frameCapFps=60, vsync=False)), encoding="utf-8")
    environment.update(OCTARYN_CLIENT_LIBRARY_ROOT=str(library),
                       OCTARYN_CLIENT_SETTINGS_PATH=str(settings),
                       OCTARYN_CLIENT_LIGHTING_PATH=str(folder / "lighting.json"),
                       OCTARYN_CLIENT_GRAPHICS_API="dx12", OCTARYN_CLIENT_UPSCALER="off",
                       OCTARYN_CLIENT_RHI_VALIDATION="1",
                       OCTARYN_CLIENT_LIVE_FRAME_TIMING="1",
                       OCTARYN_CLIENT_FRAME_TIMING_PATH=str(folder / "frame-timing.csv"),
                       OCTARYN_CLIENT_LOADING_CAPTURE_DIR=str(folder / "loading"),
                       OCTARYN_CLIENT_CAPTURE_PATH=str(folder / "frame.bmp"))
    if world:
        environment["OCTARYN_CLIENT_WORLD_PATH"] = str(world)
    environment.update(extra_environment or {})
    record_build(bundle, folder)
    command = [str(bundle / "Octaryn.Client.exe"), "--benchmark-hidden", "--benchmark-settings", *arguments]
    (folder / "command.json").write_text(json.dumps(dict(command=command, max_frame_ms=50,
        process_priority="below-normal", no_os_input=True, environment=extra_environment or {},
        require_clean_exit=require_clean_exit), indent=2), encoding="utf-8")
    with (folder / "client.log").open("wb") as log:
        result = run_capture(command, folder, environment, log, timeout=180, max_frame_ms=50,
                             require_clean_exit=require_clean_exit)
    text = (folder / "client.log").read_text(encoding="utf-8", errors="replace")
    if result != expected_exit or any(marker in text for marker in ("rmlui severity=error", "rmlui severity=warning",
            "rhi_validation severity=error", "Validation Error", "D3D12 ERROR", "world_frame_failed",
            "frame heartbeat stalled", "contract=failed")):
        raise RuntimeError(f"World-library runtime failed exit={result}: {folder}")
    return text


def saved(path):
    data = path.read_bytes()
    magic, version, length, reserved = struct.unpack_from("<4I", data)
    if magic != 0x5653475A or version != 1 or reserved or length != len(data) - 48:
        raise AssertionError("Invalid production save envelope")
    if hashlib.sha256(data[48:]).digest() != data[16:48]:
        raise AssertionError("Production save integrity differs")
    record = json.loads(data[48:])
    record["ModuleState"] = base64.b64decode(record["ModuleState"])
    return record


def gameplay(record):
    data = record["ModuleState"]
    magic, version, receipt, watermark, selected = struct.unpack_from("<IIQQI", data)
    if magic != 0x56534742 or version != 1:
        raise AssertionError("Invalid basegame gameplay snapshot")
    slots = [struct.unpack_from("<HI", data, 28 + index * 6) for index in range(10)]
    count, = struct.unpack_from("<I", data, 88)
    if count > 10000 or len(data) != 92 + 54 * count:
        raise AssertionError("Gameplay snapshot item bounds differ")
    items = [struct.unpack_from("<QHI", data, 92 + index * 54) for index in range(count)]
    return dict(receipt=receipt, watermark=watermark, selected=selected, slots=slots, items=items)


def verify_drops(snapshot):
    if snapshot["slots"][0] != (2, 6) or len(snapshot["items"]) != 2:
        raise AssertionError("Dropped item counts were not conserved in the save")
    if any((item, count) != (2, 1) for _, item, count in snapshot["items"]):
        raise AssertionError("Saved drops differ from the authoritative actions")
    total = sum(count for item, count in snapshot["slots"] if item == 2)
    total += sum(count for _, item, count in snapshot["items"] if item == 2)
    if total != 8:
        raise AssertionError("Inventory and world Apple counts are not conserved")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-bundle-root", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, default=ROOT / "logs/client/world-library")
    args = parser.parse_args()
    bundle = args.client_bundle_root.resolve()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="runtime-", dir=args.evidence_root.resolve()))
    print(f"world_library_runtime status=running evidence={evidence}", flush=True)
    fixtures(evidence)
    library = evidence / "library"
    menu = run(bundle, evidence / "menu", ["--show-worlds", "--find-worlds", str(evidence / "sources"),
                "--frames", "30", "--validate-ui", "--capture-ui", "world-library"], library)
    if any(marker not in menu for marker in ("world_library_ui_contract=passed", "loading_ui_contract=passed", "rml_ui_contract=passed",
                                              "ui_audio_contract=passed")):
        raise AssertionError("World-library document did not pass its actual contract")
    if "authoritative_player_ready" in menu or "server_world_save" in menu:
        raise AssertionError("World menu preloaded a map or authority")
    reports = {"menu": dict(lazy_menu(menu), **library_surface(menu))}
    for size in ((640, 480), (1920, 1080)):
        text = run(bundle, evidence / f"menu-{size[0]}", ["--show-worlds", "--frames", "12",
            "--capture-ui", "world-library"], library, size)
        reports[f"menu-{size[0]}"] = dict(lazy_menu(text), **library_surface(text))
    catalog_path = library / "saves/worlds/catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    worlds = catalog["worlds"]
    first_index = next(index for index, world in enumerate(worlds) if world["source"].replace("\\", "/").endswith("/1/main.glb"))
    second_index = next(index for index, world in enumerate(worlds) if world["source"].replace("\\", "/").endswith("/external/scene.gltf"))
    broken_index = next(index for index, world in enumerate(worlds) if world["source"].replace("\\", "/").endswith("/broken.glb"))
    broken = worlds[broken_index]
    text = run(bundle, evidence / "invalid-selection", ["--show-worlds", "--play-world", str(broken_index + 1),
        "--frames", "12", "--capture-ui", "world-library"], library, expected_exit=1)
    reports["invalid-selection"] = selected_loading(text, evidence / "invalid-selection", failed=True)
    if (library / "saves/worlds" / broken["id"]).exists():
        raise AssertionError("Invalid selection created a save")
    roots = []
    for name, index in (("world-a", first_index), ("world-b", second_index)):
        text = run(bundle, evidence / name, ["--show-worlds", "--play-world", str(index + 1),
                   "--frames", "360", "--validate-module-actions"], library)
        if "module_action_validation=passed" not in text:
            raise AssertionError("Actual authoritative item drops were not exercised")
        reports[name] = selected_loading(text, evidence / name)
        updated = json.loads(catalog_path.read_text(encoding="utf-8"))["worlds"][index]
        roots.append(library / "saves/worlds" / updated["id"] / updated["active_save"])
    first, second = roots
    first_before, second_before = saved(first / "world-state.save"), saved(second / "world-state.save")
    second_bytes = (second / "world-state.save").read_bytes()
    reopen = run(bundle, evidence / "world-a-reopen", ["--frames", "120"], library, world=first)
    first_after = saved(first / "world-state.save")
    before_game, after_game = gameplay(first_before), gameplay(first_after)
    if any(before_game[key] != after_game[key] for key in ("watermark", "selected", "slots", "items")):
        raise AssertionError("Authoritative inventory or world items changed during reopening")
    verify_drops(before_game)
    verify_drops(gameplay(second_before))
    if after_game["receipt"] <= before_game["receipt"] or first_after["SessionId"] <= first_before["SessionId"]:
        raise AssertionError("Reopening reused a previous receipt namespace")
    if first_after["Generation"] <= first_before["Generation"]:
        raise AssertionError("Reopened save generation did not advance")
    if (second / "world-state.save").read_bytes() != second_bytes:
        raise AssertionError("Reopening world A modified world B")
    if first_before["Player"] == second_before["Player"]:
        raise AssertionError("Translated worlds did not keep independent authoritative poses")
    updated_catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    updated_catalog["worlds"].insert(0, updated_catalog["worlds"].pop(first_index))
    catalog_path.write_text(json.dumps(updated_catalog), encoding="utf-8")
    text = run(bundle, evidence / "saved-menu", ["--show-worlds", "--frames", "12",
        "--capture-ui", "world-library"], library)
    reports["saved-menu"] = dict(lazy_menu(text), **library_surface(text))
    (evidence / "result.json").write_text(json.dumps(dict(status="passed", saves=[str(path) for path in roots],
        menu_sizes=["640x480", "1280x720", "1920x1080"], loading=reports,
        checks="zero source reads before selection, loading frames, error return, UI audio, glTF, drops, restart, isolation"), indent=2))
    print(f"world_library_runtime=passed evidence={evidence}")


if __name__ == "__main__":
    main()
