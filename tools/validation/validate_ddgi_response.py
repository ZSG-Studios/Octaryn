"""Run production Slang irradiance/sampling on CPU and compile all DDGI passes."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def reject_old_behavior(output, name, test, snap=False, presentation=False):
    generated = output / f"DDGI{name}Cpu.cpp"
    source = generated.read_text()
    if name == "Transition":
        anchor, replacement = "if(recursive_0)", f"if({'true' if presentation else 'false'})"
    elif snap:
        anchor = "lean_0 = (F32_min((0.5f), (mature_0)));"
        replacement = "lean_0 = 0.0f;"
    elif name == "Visibility":
        anchor, replacement = "(biased_0 - probePos_0)", "(probePos_0 - biased_0)"
    else:
        anchor = "return lerp_0(incoming_0, previous_1, (Vector<float, 3> )history_0);"
        replacement = """
        float brightness = incoming_0.x - previous_1.x;
        float threshold = F32_max(.025f, .5f * previous_1.x);
        if(history_0 > 0 && brightness > threshold)
            incoming_0 = previous_1 + (incoming_0 - previous_1) * (Vector<float,3>)(threshold / brightness);
        """ + anchor
    if source.count(anchor) != 1:
        raise RuntimeError(f"{name} mutation anchor changed")
    try:
        generated.write_text(source.replace(anchor, replacement))
        executable = f"ddgi_{test}_mutation.exe"
        subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/I" + str(output),
                        str(ROOT / f"tools/validation/ddgi_{test}_test.cpp"), f"/Fe:{executable}"],
                       cwd=output, check=True)
        result = subprocess.run([str(output / executable)], capture_output=True, text=True)
        if result.returncode == 0:
            raise RuntimeError(f"old {name} behavior unexpectedly passed")
        if snap and not any(message in result.stderr for message in (
                "explicit edit did not adopt bounded reactive blending",
                "warmup/reactive observation weighting changed")):
            raise RuntimeError(f"snap-on-change mutation failed for an unexpected reason: {result.stderr}")
        if name == "Transition":
            failure = "pending presentation black hole" if presentation else "removed light reentered recursive feedback"
            if failure not in result.stderr:
                raise RuntimeError(f"sampling-policy mutation failed unexpectedly: {result.stderr}")
        print(f"ddgi_{test}_{'snap_on_change' if snap else 'old_behavior'}=rejected exit={result.returncode}")
    finally:
        generated.write_text(source)


def main():
    output = ROOT / "build/release-windows/tools/ddgi-response"
    output.mkdir(parents=True, exist_ok=True)
    slang = ROOT / "build/dependencies/slang-2026.17.1/bin/slangc.exe"
    vs = find_vs_root()
    import_vs_environment(vs, "x64")
    prepend_tool_dirs(ROOT, vs, "x64")
    for name, test in (("Response", "response"), ("Visibility", "visibility"), ("Transition", "transition")):
        subprocess.run([str(slang), str(ROOT / f"tools/validation/DDGI{name}Probe.slang"),
                        "-entry", "main", "-target", "cpp", "-o", str(output / f"DDGI{name}Cpu.cpp")], check=True)
        subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/I" + str(output),
                        str(ROOT / f"tools/validation/ddgi_{test}_test.cpp"), f"/Fe:ddgi_{test}_test.exe"],
                       cwd=output, check=True)
        subprocess.run([str(output / f"ddgi_{test}_test.exe")], check=True)
        reject_old_behavior(output, name, test)
        if name == "Response":
            reject_old_behavior(output, name, test, snap=True)
        if name == "Transition":
            reject_old_behavior(output, name, test, presentation=True)
    for name in ("DDGITrace", "DDGIUpdate", "DDGISeed"):
        shader = ROOT / f"octaryn-client/Shaders/DDGI/{name}.slang"
        for target, suffix, flags in (
            ("spirv", "spv", ["-profile", "spirv_1_5"]),
            ("dxil", "dxil", ["-profile", "sm_6_6", "-dxc-path",
                             str(ROOT / "build/dependencies/slang-rhi-windows-x64-Release/_deps/dxc-src/bin/x64")]),
            ("metal", "metal", []),
        ):
            subprocess.run([str(slang), str(shader), "-entry", "main", "-target", target,
                            *flags, "-o", str(output / f"{name}.{suffix}")], check=True)
    print("ddgi_response_targets=SPIR-V,DXIL,Metal gpu_runtime=not_run")


if __name__ == "__main__":
    main()
