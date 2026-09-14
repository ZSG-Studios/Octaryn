"""Qualify packaged RmlUi documents and real RHI rendering without input injection."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import struct
import tempfile

from validate_rhi_client_diagnostic import inspect_result, native_backend


def run_case(bundle, evidence, name, arguments, size=(1280, 720), backend=None, upscaler=None, settings_override=None, capture_temporal=False):
    case = evidence / name
    case.mkdir()
    world = case / "world"
    world.mkdir()
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("OCTARYN_CLIENT_", "OCTARYN_SERVER_"))}
    backend = backend or native_backend()
    upscaler = upscaler or "off"
    settings = case / "settings.json"
    saved_settings = {"windowWidth": size[0], "windowHeight": size[1],
                      "fullscreen": False, "renderDistance": 4}
    saved_settings.update(settings_override or {})
    settings.write_text(json.dumps(saved_settings), encoding="utf-8")
    environment.update({
        "OCTARYN_CLIENT_WORLD_PATH": str(world),
        "OCTARYN_CLIENT_LIGHTING_PATH": str(case / "lighting.json"),
        "OCTARYN_CLIENT_INVENTORY_PATH": str(case / "build-palette.json"),
        "OCTARYN_CLIENT_SETTINGS_PATH": str(settings),
        "OCTARYN_CLIENT_GRAPHICS_API": backend,
        "OCTARYN_CLIENT_UPSCALER": upscaler,
        "OCTARYN_CLIENT_RHI_VALIDATION": "1",
        "OCTARYN_CLIENT_CAPTURE_PATH": str(case / "frame.bmp"),
        "OCTARYN_CLIENT_PROFILE_PATH": str(case / "world-profile.csv"),
    })
    if capture_temporal:
        environment["OCTARYN_CLIENT_CAPTURE_TEMPORAL"] = "1"
    if backend == "vulkan":
        environment["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
        if os.name == "nt":
            layers = Path(__file__).resolve().parents[2] / "build/dependencies/vulkan-validation"
            if not (layers / "VkLayer_khronos_validation.json").is_file() or not (layers / "vk_layer_settings.txt").is_file():
                raise RuntimeError("Pinned Vulkan validation layer/settings are missing")
            environment.update(VK_LAYER_PATH=str(layers), VK_LAYER_SETTINGS_PATH=str(layers))
    executable = bundle / ("Octaryn.Client.exe" if os.name == "nt" else "Octaryn.Client")
    with (case / "client.log").open("wb") as log:
        process = subprocess.Popen(
            [str(executable), "--frames", "600", "--validate-ui", "--benchmark-hidden", *arguments],
            cwd=case, env=environment, stdout=log, stderr=subprocess.STDOUT,
        )
        try:
            result = process.wait(timeout=180)
        except subprocess.TimeoutExpired:
            runtime = world / "runtime"
            runtime.mkdir(exist_ok=True)
            (runtime / "shutdown.request").write_text("stop\n", encoding="utf-8")
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=5)
            raise RuntimeError(f"RmlUi {name} runtime timed out: {case}")
    text = (case / "client.log").read_text(encoding="utf-8", errors="replace")
    frames, columns, quads = inspect_result(result, text, {"dx12": "D3D12", "vulkan": "Vulkan", "metal": "Metal"}[backend])
    if upscaler != "off" and "world_fsr2 version=2.2.1" not in text:
        raise RuntimeError(f"RmlUi {name} omitted requested actual FSR2 pipeline: {case}")
    if "rml_ui_contract=passed" not in text:
        raise RuntimeError(f"RmlUi {name} omitted document validation: {case}")
    if "rml_ui_inventory_contract=passed" not in text:
        raise RuntimeError(f"RmlUi {name} omitted inventory interaction validation: {case}")
    if "rml_ui renderer=slang-rhi frame=" not in text:
        raise RuntimeError(f"RmlUi {name} did not submit UI geometry to Slang RHI: {case}")
    if any(marker in text.lower() for marker in (
        "rmlui severity=error", "rmlui severity=warning", "rml_ui_contract=failed",
        "rml_ui_inventory_contract=failed", "rmlui image failed",
    )):
        raise RuntimeError(f"RmlUi {name} reported a document or rendering failure: {case}")
    capture = case / "frame.bmp"
    if not capture.is_file() or capture.stat().st_size < 1024 or "world_capture" not in text:
        raise RuntimeError(f"RmlUi {name} did not capture the presented GPU frame: {case}")
    width, height = struct.unpack_from("<ii", capture.read_bytes(), 18)
    if (width, abs(height)) != size:
        raise RuntimeError(f"RmlUi {name} capture size differs from requested {size}: {width}x{height}")
    print(f"rml_ui_case=passed surface={name} frames={frames} columns={columns} quads={quads} evidence={case}", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client-bundle-root", required=True, type=Path)
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--backend", choices=("dx12", "vulkan", "metal"), default=native_backend())
    parser.add_argument("--upscaler", choices=("off", "native", "quality", "balanced", "performance", "ultra-performance"), default="off")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--surface", choices=("all", "hud", "settings", "lighting", "diagnostics",
                                            "inventory", "creative", "pause"), default="all")
    args = parser.parse_args()
    if not 640 <= args.width <= 7680 or not 480 <= args.height <= 4320:
        parser.error("resolution must be between 640x480 and 7680x4320")
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="rml-", dir=args.evidence_root.resolve()))
    surfaces = {"hud": ["--third-person"], "settings": ["--show-settings"],
                "lighting": ["--show-lighting"], "diagnostics": ["--show-diagnostics"],
                "inventory": ["--show-inventory"], "creative": ["--show-creative"],
                "pause": ["--show-menu"]}
    for name, arguments in surfaces.items():
        if args.surface in ("all", name):
            run_case(args.client_bundle_root.resolve(), evidence, name, arguments,
                     (args.width, args.height), args.backend, args.upscaler)
    print(f"rml_ui_validation=passed evidence={evidence}")


if __name__ == "__main__":
    main()
