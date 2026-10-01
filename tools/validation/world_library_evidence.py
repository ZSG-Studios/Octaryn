"""Validate positive lazy-menu and selected-loading evidence from the real client."""
import re
import struct


IO_FIELDS = ("source_parses", "resource_hashes", "model_loads", "prepared_catalog_reads")
SHOWN = "world_loading_screen status=shown source_work_dispatched=0"
STAGES = {"Reading map": 1, "Optimizing geometry": 1, "Uploading textures": 0,
          "Uploading materials": 0, "Preparing geometry": 1, "Uploading geometry": 0,
          "Preparing graphics": 0, "Preparing presentation": 0, "Preparing player collision": 1}


def library_surface(text):
    rows = re.findall(r"^world_library_surface visible=(\d+) loading=(\d+)$", text, re.MULTILINE)
    if not rows or any(row != ("1", "0") for row in rows):
        raise AssertionError("Captured menu did not show the library with loading hidden")
    return dict(captured_library_visible=True, loading_overlay_hidden=True)


def lazy_menu(text):
    rows = re.findall(r"^world_library_io phase=menu (.+)$", text, re.MULTILINE)
    if not rows:
        raise AssertionError("Menu did not report source access counters")
    for row in rows:
        values = dict(re.findall(r"(\w+)=(\d+)", row))
        if any(values.get(field) != "0" for field in IO_FIELDS):
            raise AssertionError("Unselected menu inspected source content: " + row)
    unselected = text.split(SHOWN, 1)[0]
    if any(marker in unselected for marker in ("world_load_stage", "map_startup stage=", "map_renderer_loaded",
                                                "world_scene_selected", "authoritative_player_ready", "server_world_save")):
        raise AssertionError("Unselected menu loaded a map or authoritative session")
    return dict(menu_samples=len(rows), unselected_content_reads=0)


def image(path):
    if not path.is_file():
        raise AssertionError("Loading frame was not captured: " + str(path))
    with path.open("rb") as file:
        header = file.read(54)
    if len(header) != 54 or header[:2] != b"BM":
        raise AssertionError("Invalid loading capture: " + str(path))
    width, height = struct.unpack_from("<ii", header, 18)
    if width < 640 or abs(height) < 480:
        raise AssertionError("Loading capture was below the supported menu size")
    return str(path)


def selected_loading(text, folder, failed=False):
    result = lazy_menu(text)
    if SHOWN not in text:
        raise AssertionError("Selected loading was not shown before dispatch")
    shown = text.index(SHOWN)
    captures = [image(folder / "loading/Checking-the-selected-world---.bmp")]
    if failed:
        failure = text.find("status=failed", shown)
        if failure < shown or "world_load_stage" in text or "authoritative_player_ready" in text:
            raise AssertionError("Invalid selected source did not return safely to the library")
        captures.append(image(folder / "frame.bmp"))
        return dict(result, **library_surface(text), outcome="failed selection returned to library", captures=captures)
    rows = list(re.finditer(r"^world_load_stage stage=(.+) ui_safe=(\d+)$", text, re.MULTILINE))
    if not rows or rows[0].start() <= shown:
        raise AssertionError("Native map loading preceded its loading screen")
    stages = {row.group(1): int(row.group(2)) for row in rows}
    for stage, cpu in STAGES.items():
        if stages.get(stage) != cpu:
            raise AssertionError("Missing or unsafe loading ownership stage: " + stage)
        captures.append(image(folder / "loading" / (stage.replace(" ", "-") + ".bmp")))
    if "map_boot responsiveness=1" not in text or "result=ready" not in text:
        raise AssertionError("Map loading did not join successfully")
    return dict(result, outcome="selected world loaded", stages=stages, captures=captures)
