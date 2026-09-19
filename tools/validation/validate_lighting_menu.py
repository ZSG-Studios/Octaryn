"""Check authored lighting menu metadata and persisted/source limits without a GPU."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]


def main():
    document = ET.parse(ROOT / "octaryn-basegame/Assets/Ui/game.rml")
    ids = [element.attrib["id"] for element in document.iter() if "id" in element.attrib]
    assert len(ids) == len(set(ids)), "duplicate UI IDs"
    elements = {element.attrib["id"]: element for element in document.iter() if "id" in element.attrib}
    parents = {child: parent for parent in document.iter() for child in parent}
    controls = ROOT / "octaryn-client/Source/Ui/GameUi"
    events = (controls / "GameUiEvents.cpp").read_text()
    update = (controls / "GameUiUpdate.cpp").read_text()
    saved = (ROOT / "octaryn-client/Source/Settings/AppSettings/AppSettings.cpp").read_text()
    backend = (ROOT / "octaryn-client/Source/Rendering/RenderBackend/LightingSystem.cpp").read_text()
    persistence = (ROOT / "octaryn-client/Source/Settings/RuntimeSettings/RuntimeSettings.cpp").read_text()
    ranges = (("live-gi-voxel", "gi_voxel_radius", "giVoxelRadius", 32),
              ("live-gi-coarse", "gi_coarse_radius", "giCoarseRadius", 1024),
              ("live-shadow-distance", "shadow_distance", "shadowDistance", 1024),
              ("live-reflection-distance", "reflection_distance", "reflectionDistance", 1024))
    for binding, (id_, field, key, maximum) in enumerate(ranges):
        slider, number = elements[id_], elements[id_ + "-number"]
        assert slider.attrib["type"] == "range" and number.attrib["type"] == "text", id_
        assert slider.attrib["live"] == number.attrib["live"] == str(binding), id_
        assert (slider.attrib["min"], slider.attrib["max"], slider.attrib["step"]) == ("0", str(maximum), "1"), id_
        assert parents[slider].attrib.get("title") and parents[slider].find("label").attrib["for"] == id_, id_
        assert id_ + "-value" in elements and f'"{id_}"' in update, id_
        assert re.search(rf"settings->{field}\s*=\s*std::min<uint16_t>\(settings->{field},\s*{maximum}u\)", saved), field
        assert f"file.{key} = settings.{field};" in persistence, key
        assert f"controls->{field} = settings.{field};" in persistence, key
        assert f"settings.{field} = controls->{field};" in persistence, key
    assert "std::min(voxel_radius,32u)" in backend and "std::min(coarse_radius,1024u)" in backend
    assert "*staged[live]=*fields[live]" in events, "live changes must survive later display Apply"
    assert "controls.display_menu.ray_tracing_enabled=controls.ray_tracing_enabled" in events
    assert "std::lround(std::clamp(number,0.f,high[live]))" in events, "clamp before integer conversion"
    quality = elements["lighting-quality"]
    assert quality.find("span").text == "GI performance"
    assert quality.attrib["action"] == "cycle-lighting-quality", "preserve saved quality binding"
    targets = re.findall(r"(Low|Medium|High|Ultra) (0\.\d+) ms", quality.attrib["title"])
    assert targets == [("Low", "0.12"), ("Medium", "0.22"), ("High", "0.35"), ("Ultra", "0.50")]
    quality_source = (ROOT / "octaryn-client/Source/Rendering/RenderBackend/LightingQuality.h").read_text()
    for tier, value in targets:
        branch = "default" if tier == "High" else f"case LightingQuality::{tier}"
        assert re.search(rf"{branch}:\s*return\s*{re.escape(value[1:])};", quality_source), tier
    assert "per enabled volume per 60 Hz tick" in quality.attrib["title"]
    assert "bounded adaptive scheduling" in quality.attrib["title"]
    assert "not an FPS guarantee" in elements["gi-performance-help"].text
    assert "resident ray-scene geometry" in elements["gi-range-help"].text
    views = (ROOT / "octaryn-client/Source/Ui/LightingPanel/LightingDebugViews.h").read_text()
    assert [int(value) for value in re.findall(r"LightingDebugView\{(\d+),", views)] == list(range(31))
    lighting = (ROOT / "octaryn-client/Source/Settings/LightingSettings/LightingSettings.cpp").read_text()
    for key, value in (("AmbientStrength", "0.65"), ("SunStrength", "0.75"),
                       ("FogDistance", "1024.0"), ("SkylightFloor", "0.25")):
        assert f"kDefault{key} = {value}f" in lighting, "screenshot lighting default changed"
    for path in (controls / "GameUiEvents.cpp", controls / "GameUiUpdate.cpp", controls / "GameUiValidation.cpp"):
        assert len(path.read_text().splitlines()) <= 500, path
    print("lighting_menu_source_contract=passed ranges=4 debug_ids=31 "
          "near_max=32 far_max=1024 units=blocks defaults=preserved os_events=0 gpu_devices=0")


if __name__ == "__main__":
    main()
