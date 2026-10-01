"""Cancel actual hidden loading phases through the product UI action, without OS input."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

from validate_world_library import fixtures
from validate_world_library_runtime import run
from world_library_evidence import IO_FIELDS, image, lazy_menu, library_surface

ROOT = Path(__file__).resolve().parents[2]
CASES = (("queued", "Checking the selected world...", 1),
         ("authority", "Starting world", 2),
         ("geometry", "Preparing geometry", 1),
         ("player", "Waiting for player", 1),
         ("player-wait", "Waiting for player", 2))


def hashes(paths, root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.is_file()}


def cancellation_evidence(text, folder, stage, occurrence):
    result = dict(lazy_menu(text), **library_surface(text))
    marker = f"world_loading_validation_cancel stage={stage} occurrence={occurrence}"
    if text.count(marker) != 1:
        raise AssertionError("Cancellation action was not dispatched exactly once: " + stage)
    if "capture_clean_exit=passed active_owned_processes=0 before_safety_cleanup=1" not in text:
        raise AssertionError("Authority process exit was not verified before safety cleanup")
    player_wait = stage == "Waiting for player" and occurrence == 2
    if not player_wait and ("world_scene_selected" in text or "authoritative_player_ready" in text):
        raise AssertionError("Canceled load entered the game session")
    if stage == "Checking the selected world...":
        rows = re.findall(r"^world_library_io phase=exit (.+)$", text, re.MULTILINE)
        if not rows or any(dict(re.findall(r"(\w+)=(\d+)", row)).get(field) != "0"
                           for row in rows for field in IO_FIELDS):
            raise AssertionError("Queued cancellation performed source work")
        if "map_boot responsiveness=" in text:
            raise AssertionError("Queued cancellation reached map startup")
    elif stage == "Starting world":
        if "map_boot responsiveness=" in text:
            raise AssertionError("Canceled server startup reached map startup")
        if "world_session_start result=cancelled server_running=0" not in text:
            raise AssertionError("Canceled authority startup was not joined and stopped")
    elif player_wait:
        if "world_loading_cancel phase=player_wait" not in text or "world_scene_selected" not in text:
            raise AssertionError("Cancellation did not reach the actual session player wait")
        if not re.search(r"^map_boot responsiveness=1 .*result=ready$", text, re.MULTILINE):
            raise AssertionError("Player-wait cancellation did not follow completed map loading")
    else:
        if not re.search(r"^map_boot responsiveness=1 .*result=cancelled$", text, re.MULTILINE):
            raise AssertionError("Canceled map worker did not join before returning")
        if "world_load_return_menu" not in text:
            raise AssertionError("Canceled map load did not return to the library")
        if "world_loading_cleanup server_running=0 result=cancelled" not in text:
            raise AssertionError("Canceled map load did not stop authority")
    name = re.sub(r"[^a-zA-Z0-9]", "-", stage)
    captures = [image(folder / "loading" / (name + ".bmp")), image(folder / "frame.bmp")]
    return dict(result, stage=stage, occurrence=occurrence, captures=captures,
                no_surviving_authority=True, watchdog_frame_ms=50)


def prepare(probe, case):
    fixture = case / "fixture"
    fixture.mkdir(parents=True)
    fixtures(fixture)
    completed = subprocess.run([str(probe), str(fixture)], capture_output=True, text=True,
                               encoding="utf-8", errors="replace", timeout=120)
    (case / "native-setup.log").write_text(completed.stdout + completed.stderr, encoding="utf-8")
    completed.check_returncode()
    library = fixture / "state"
    catalog = library / "saves/worlds/catalog.json"
    worlds = json.loads(catalog.read_text(encoding="utf-8"))["worlds"]
    index = next(i for i, world in enumerate(worlds) if world["source"].replace("\\", "/").endswith("/2/main.glb"))
    target = worlds[index]
    if target["active_save"] or target["resources"]:
        raise AssertionError("Cancellation target was already opened by setup")
    saves = catalog.parent
    guard_ids = [world["id"] for world in worlds if world["id"] != target["id"]]
    guard_records = {world["id"]: world for world in worlds if world["id"] in guard_ids}
    guards = hashes((file for id_ in guard_ids for file in (saves / id_).rglob("*")), saves)
    if not guards:
        raise AssertionError("Cancellation setup has no independent saves to protect")
    sources = hashes((path for path in (fixture / "sources").rglob("*") if
                      path.suffix.lower() in (".glb", ".gltf", ".bin", ".png", ".json")), fixture)
    return library, catalog, index, target, guard_ids, guards, sources, guard_records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-bundle-root", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, default=ROOT / "logs/client/world-loading-cancel")
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="cancel-", dir=args.evidence_root.resolve()))
    print(f"world_loading_cancel status=running evidence={output}", flush=True)
    results = {}
    report = dict(status="failed", evidence=str(output), cases=results, no_os_input=True)
    try:
        for name, stage, occurrence in CASES:
            case = output / name
            library, catalog, index, target, guard_ids, guards, sources, guard_records = prepare(args.probe.resolve(), case)
            text = run(args.client_bundle_root.resolve(), case / "client",
                ["--show-worlds", "--play-world", str(index + 1), "--frames", "12", "--capture-ui", "world-library"],
                library, extra_environment={"OCTARYN_CLIENT_LOADING_CANCEL_STAGE": stage,
                    "OCTARYN_CLIENT_LOADING_CANCEL_OCCURRENCE": str(occurrence)}, require_clean_exit=True)
            result = cancellation_evidence(text, case / "client", stage, occurrence)
            saves = catalog.parent
            after = hashes((file for id_ in guard_ids for file in (saves / id_).rglob("*")), saves)
            if guards != after:
                raise AssertionError("Cancel changed an independent saved world")
            records = {world["id"]: world for world in json.loads(catalog.read_text(encoding="utf-8"))["worlds"]
                       if world["id"] in guard_ids}
            if records != guard_records:
                raise AssertionError("Cancel changed an independent world's source or save selection")
            if sources != hashes((case / "fixture" / path for path in sources), case / "fixture"):
                raise AssertionError("Cancel changed original world content")
            if name == "queued":
                world = next(world for world in json.loads(catalog.read_text(encoding="utf-8"))["worlds"]
                             if world["id"] == target["id"])
                if world["resources"] or world["active_save"] or (saves / target["id"]).exists():
                    raise AssertionError("Queued cancel published validation or a save")
            results[name] = dict(result, isolated_save_files=len(guards), isolated_world_records=len(guard_records),
                                 original_sources_unchanged=len(sources))
        report["status"] = "passed"
    finally:
        (output / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"world_loading_cancel=passed evidence={output}")


if __name__ == "__main__":
    main()
