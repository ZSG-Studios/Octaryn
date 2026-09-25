"""Verify persisted player FSR controls against captured production dispatch state."""
import argparse
import json
from pathlib import Path
from validate_rml_ui import run_case


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-bundle-root", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    cases = [
        ("custom-dx12", "dx12", "custom", 6, False, False),
        ("dynamic-dx12", "dx12", "custom", 6, True, True),
        ("dynamic-vulkan", "vulkan", "custom", 6, True, True),
        ("native-dx12", "dx12", "native", 1, True, True),
    ]
    for name, api, mode, mode_id, dynamic, sharpen in cases:
        run_case(args.client_bundle_root.resolve(), args.evidence_root.resolve(), name,
                 ["--third-person"], backend=api, upscaler=mode, capture_temporal=True,
                 settings_override={"upscalerMode": mode_id, "fsrRenderScale": .725,
                                    "fsrDynamicResolution": int(dynamic), "fsrMinScale": .45,
                                    "fsrMaxScale": .85, "fsrTargetFps": 120,
                                    "fsrSharpening": int(sharpen), "fsrSharpness": .73})
        data = json.loads((args.evidence_root / name / "frame.bmp.temporal.json").read_text())
        assert data["mode"] == mode_id, (name, data)
        assert bool(data["sharpening"]) == sharpen and abs(data["sharpness"] - .73) < .001, (name, data)
        assert bool(data["dynamic_active"]) == (dynamic and mode_id != 1), (name, data)
        if mode_id == 1:
            assert (data["render_width"], data["render_height"]) == (1280, 720), data
        elif dynamic:
            assert .449 <= data["render_scale"] <= .851 and data["target_fps"] == 120, data
            assert abs(data["render_scale"] - .725) > .005, ("GPU controller did not adjust scale", data)
            scene = next(image for image in data["images"] if image["name"] == "scene")
            assert (scene["width"], scene["height"]) == (1088, 612), scene
        else:
            assert (data["render_width"], data["render_height"]) == (928, 522), data
        print(f"fsr_player_settings=passed case={name} mode={mode_id} scale={data['render_scale']} sharpening={sharpen}", flush=True)


if __name__ == "__main__":
    main()
