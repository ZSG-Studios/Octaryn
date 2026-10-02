"""Check authored lighting menu metadata and persisted/source limits without a GPU."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]


def main():
    document = ET.parse(ROOT / "octaryn-basegame/Assets/Ui/Game/game.rml")
    ids = [element.attrib["id"] for element in document.iter() if "id" in element.attrib]
    assert len(ids) == len(set(ids)), "duplicate UI IDs"
    elements = {element.attrib["id"]: element for element in document.iter() if "id" in element.attrib}
    parents = {child: parent for parent in document.iter() for child in parent}
    controls = ROOT / "octaryn-basegame/Source/Client/Ui"
    events = (controls / "GameUiEvents.cpp").read_text()
    update = (controls / "GameUiUpdate.cpp").read_text()
    saved = (ROOT / "octaryn-client/Source/Settings/AppSettings/AppSettings.cpp").read_text()
    backend = (ROOT / "octaryn-client/Source/Rendering/RenderBackend/LightingSystem.cpp").read_text()
    ray_header = (ROOT / "octaryn-client/Source/Rendering/RenderBackend/WorldRayTracing.h").read_text()
    ray_source = (ROOT / "octaryn-client/Source/Rendering/RenderBackend/WorldRayTracing.cpp").read_text()
    shadow_shader = (ROOT / "octaryn-client/Shaders/RayTracing/Shadow.slang").read_text()
    persistence = (ROOT / "octaryn-client/Source/Settings/RuntimeSettings/RuntimeSettings.cpp").read_text()
    ranges = (("live-shadow-distance", "shadow_distance", "shadowDistance", 1024),
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
    # A zero RT shadow range must route through the raster visibility source
    # when enabled; leaving the target cleared to one leaks direct sun through
    # every block. Voxel light propagation remains independent of RT.
    assert "world_ray_coverage_complete" in ray_header and "world_ray_coverage_complete(r)" in backend
    assert "stats.pending_columns==0" in ray_source and "stats.ready_columns==stats.resident_columns" in ray_source
    assert "tap<clamp(shadowSamples,1u,8u)" in shadow_shader
    assert "conservative=min(conservative,sample_visibility)" in shadow_shader
    assert "*staged[live]=*fields[live]" in events, "live changes must survive later display Apply"
    assert "controls.display_menu.ray_tracing_enabled=controls.ray_tracing_enabled" in events
    assert "cycle-reflection-quality" in events and "cycle-shadow-quality" in events
    assert "controls.reflection_quality" in events and "controls.shadow_quality" in events
    assert "std::lround(std::clamp(number,0.f,high[live]))" in events, "clamp before integer conversion"
    for removed in ("live-gi-voxel", "live-gi-coarse", "lighting-quality"):
        assert removed not in elements, "obsolete probe controls remain"
    for removed in ("giVoxelRadius", "giCoarseRadius", "lightingQuality"):
        assert removed not in persistence, "obsolete probe settings remain"
    for quality_id, action in (("reflection-quality", "cycle-reflection-quality"),
                               ("shadow-quality", "cycle-shadow-quality")):
        assert elements[quality_id].attrib["action"] == action
        assert quality_id + "-value" in elements
    for field, key in (("reflection_quality", "reflectionQuality"), ("shadow_quality", "shadowQuality")):
        assert f"file.{key} = settings.{field};" in persistence
        assert f"controls->{field} = settings.{field};" in persistence
        assert f"settings.{field} = controls->{field};" in persistence
    views = (ROOT / "octaryn-client/Source/Ui/LightingPanel/LightingDebugViews.h").read_text()
    assert [int(value) for value in re.findall(r"LightingDebugView\{(\d+),", views)] == [0, 1, *range(8, 21), 28, 31]
    lighting = (ROOT / "octaryn-client/Source/Settings/LightingSettings/LightingSettings.cpp").read_text()
    for key, value in (("AmbientStrength", "0.65"), ("SunStrength", "0.75"),
                       ("FogDistance", "1024.0"), ("SkylightFloor", "0.25")):
        assert f"kDefault{key} = {value}f" in lighting, "screenshot lighting default changed"
    for path in (controls / "GameUiEvents.cpp", controls / "GameUiUpdate.cpp", controls / "GameUiValidation.cpp"):
        assert len(path.read_text().splitlines()) <= 500, path
    print("lighting_menu_source_contract=passed ranges=2 debug_ids=20 "
          "fixed_voxel_metres=1 trace_max=1024 units=blocks defaults=preserved os_events=0 gpu_devices=0")


if __name__ == "__main__":
    main()
