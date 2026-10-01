"""Capture real menu/card GPU output with isolated settings on both Windows RHIs."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from capture_regions import read_bmp, save_png
from case_evidence import record_build
from validate_rhi_client_diagnostic import inspect_result


ROOT = Path(__file__).resolve().parents[2]
ACTIVITY = ("layer_pushes", "filter_passes", "blur_passes", "shadow_passes")


def isolated_environment(case, backend, width, height):
    environment = {key: value for key, value in os.environ.items()
                   if not key.upper().startswith("OCTARYN_")}
    settings = case / "settings.json"
    settings.write_text(json.dumps({"windowWidth": width, "windowHeight": height,
                                   "fullscreen": False, "renderDistance": 4,
                                   "upscalerMode": 0, "fsrDynamicResolution": 0}),
                        encoding="utf-8")
    world = case / "world"
    world.mkdir()
    environment.update({
        "OCTARYN_CLIENT_WORLD_PATH": str(world),
        "OCTARYN_CLIENT_SERVER_LOG_DIR": str(world / "logs/server"),
        "OCTARYN_CLIENT_SETTINGS_PATH": str(settings),
        "OCTARYN_CLIENT_LIGHTING_PATH": str(case / "lighting.json"),
        "OCTARYN_CLIENT_INVENTORY_PATH": str(case / "inventory.json"),
        "OCTARYN_CLIENT_PROFILE_PATH": str(case / "profile.csv"),
        "OCTARYN_CLIENT_GRAPHICS_API": backend,
        "OCTARYN_CLIENT_UPSCALER": "off",
        "OCTARYN_CLIENT_GI": "direct",
        "OCTARYN_CLIENT_RHI_VALIDATION": "1",
    })
    if backend == "vulkan":
        sdk = environment.get("VULKAN_SDK")
        if not sdk:
            raise RuntimeError("VULKAN_SDK must identify the installed Vulkan validation SDK")
        layers = Path(sdk) / "Bin"
        if not (layers / "VkLayer_khronos_validation.json").is_file():
            raise RuntimeError(f"Vulkan SDK validation layer missing: {layers}")
        environment.update(VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation",
                           VK_LAYER_PATH=str(layers), VK_LAYER_SETTINGS_PATH=str(layers))
    return environment


def stop_timed_out_client(process, case):
    runtime = case / "world/runtime"
    runtime.mkdir(parents=True, exist_ok=True)
    (runtime / "shutdown.request").write_text("stop\n", encoding="utf-8")
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def run_case(bundle, evidence, backend, surface, args):
    case = evidence / f"{backend}-{surface}"
    case.mkdir()
    executable = bundle / "Octaryn.Client.exe"
    environment = isolated_environment(case, backend, args.width, args.height)
    record_build(bundle, case)
    if args.full_size_surfaces:
        environment["OCTARYN_CLIENT_UI_FULL_SIZE_SURFACES"] = "1"
    capture_name = f"effects-{evidence.name}-{backend}-{surface}"
    command = [str(executable), "--frames", "320", "--validate-ui",
               "--benchmark-hidden", "--benchmark-settings", "--capture-ui", capture_name,
               "--show-menu" if surface == "menu" else "--show-item-target"]
    (case / "command.json").write_text(json.dumps(command, indent=2), encoding="utf-8")
    print(f"ui_effects_case=running backend={backend} surface={surface} evidence={case}", flush=True)
    with (case / "client.log").open("wb") as output:
        process = subprocess.Popen(command, cwd=case, env=environment,
                                   stdout=output, stderr=subprocess.STDOUT)
        try:
            returncode = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            stop_timed_out_client(process, case)
            raise RuntimeError(f"UI effects capture timed out: {case}")
    text = (case / "client.log").read_text(encoding="utf-8", errors="replace")
    captures = re.findall(r"ui_capture path=(.*?) size=(\d+)x(\d+)", text)
    if not captures:
        raise RuntimeError(f"No GPU UI capture recorded (exit={returncode}): {case}")
    capture_path, width, height = captures[-1]
    source = Path(capture_path)
    if source.name != capture_name + ".bmp" or not source.is_file():
        raise RuntimeError(f"Unexpected or missing UI capture: {source}")
    capture = case / "ui.bmp"
    shutil.copy2(source, capture)
    _, _, actual_width, actual_height, _ = read_bmp(capture)
    if (actual_width, actual_height) != (int(width), int(height)):
        raise RuntimeError(f"Capture dimensions disagree with GPU log: {case}")
    if (actual_width, actual_height) != (args.width, args.height):
        raise RuntimeError(f"Capture did not use the requested {args.width}x{args.height} resolution: {case}")
    save_png(capture, case / "ui.png")
    api = {"dx12": "D3D12", "vulkan": "Vulkan"}[backend]
    frames, primitives, _ = inspect_result(returncode, text, api, minimum_frames=320)
    for marker in ("rml_ui_contract=passed", "item_target_contract=passed"):
        if marker not in text:
            raise RuntimeError(f"Missing {marker}: {case}")
    forbidden = ("rmlui severity=error", "rmlui severity=warning", "rmlui image failed",
                 "rml_ui_contract=failed", "item_target_contract=failed", "ui capture failed")
    if any(marker in text.lower() for marker in forbidden):
        raise RuntimeError(f"UI validation failure: {case}")
    renderer_lines = [line for line in text.splitlines() if line.startswith("rml_ui renderer=")]
    activity = {key: max((int(value) for line in renderer_lines
                         for value in re.findall(rf"\b{key}=(\d+)", line)), default=0)
                for key in (*ACTIVITY, "clip_writes", "mask_passes")}
    required = (*ACTIVITY, "clip_writes") if surface == "card" else ACTIVITY
    if any(activity[key] <= 0 for key in required):
        raise RuntimeError(f"Effects were not positively exercised: {activity}: {case}")
    result = dict(backend=backend, surface=surface, full_size_surfaces=args.full_size_surfaces,
                  frames=frames, primitives=primitives,
                  activity=activity, capture=str(capture), source_capture=str(source),
                  size=[actual_width, actual_height], exit_code=returncode)
    result['filter_allocations'] = [dict((key, int(value)) for key, value in re.findall(r'(\w+)=(\d+)', line))
                                    for line in text.splitlines() if line.startswith('rml_filter_memory ')]
    (case / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"ui_effects_case=passed backend={backend} surface={surface} "
          f"activity={json.dumps(activity)} evidence={case}", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-bundle-root", type=Path,
                        default=ROOT / "build/release-windows/client/bundle")
    parser.add_argument("--evidence-root", type=Path, default=ROOT / "logs/client/ui-effects")
    parser.add_argument("--backend", choices=("all", "dx12", "vulkan"), default="all")
    parser.add_argument("--surface", choices=("all", "menu", "card"), default="all")
    parser.add_argument("--width", type=int, default=2560)
    parser.add_argument("--height", type=int, default=1440)
    parser.add_argument("--timeout", type=int, default=240)
    parser.add_argument("--full-size-surfaces", action="store_true", help="Reference frame-sized UI filter targets")
    args = parser.parse_args()
    if not 640 <= args.width <= 7680 or not 480 <= args.height <= 4320 or args.timeout < 1:
        parser.error("Invalid capture resolution or timeout")
    bundle = args.client_bundle_root.resolve()
    if not (bundle / "Octaryn.Client.exe").is_file():
        parser.error(f"Client bundle missing: {bundle}")
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="run-", dir=args.evidence_root.resolve()))
    backends = ("dx12", "vulkan") if args.backend == "all" else (args.backend,)
    surfaces = ("menu", "card") if args.surface == "all" else (args.surface,)
    results = [run_case(bundle, evidence, backend, surface, args)
               for backend in backends for surface in surfaces]
    (evidence / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    print(f"ui_effects_validation=passed cases={len(results)} evidence={evidence}")


if __name__ == "__main__":
    main()
