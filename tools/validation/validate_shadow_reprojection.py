"""Execute production sun-history geometry on CPU; compile GPU shaders only."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / "build/release-windows/tools/shadow-reprojection"
    output.mkdir(parents=True, exist_ok=True)
    slang = ROOT / "build/dependencies/slang-2026.17.1/bin/slangc.exe"
    subprocess.run([str(slang), str(ROOT / "tools/validation/ShadowReprojectionProbe.slang"),
                    "-entry", "main", "-target", "cpp", "-o", str(output / "ShadowReprojection.cpp")], check=True)
    shader = ROOT / "octaryn-client/Shaders/Shadows/Temporal.slang"
    for target, suffix, flags in (
        ("spirv", "spv", ["-profile", "spirv_1_5"]),
        ("dxil", "dxil", ["-profile", "sm_6_0", "-dxc-path",
                          str(ROOT / "build/dependencies/slang-rhi-windows-x64-Release/_deps/dxc-src/bin/x64")]),
        ("metal", "metal", []),
    ):
        subprocess.run([str(slang), str(shader), "-entry", "main", "-target", target,
                        *flags, "-o", str(output / f"Temporal.{suffix}")], check=True)
    vs = find_vs_root()
    import_vs_environment(vs, "x64")
    prepend_tool_dirs(ROOT, vs, "x64")
    subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/I" + str(output),
                    str(ROOT / "tools/validation/shadow_reprojection_test.cpp"), "/Fe:shadow_reprojection_test.exe"],
                   cwd=output, check=True)
    subprocess.run([str(output / "shadow_reprojection_test.exe")], check=True)
    # The old point-distance comparison must fail the same geometry oracle.
    generated = output / "ShadowReprojection.cpp"
    source = generated.read_text()
    old = "previousEye_0 + previousRay_0 * (Vector<float, 3> )depth_0"
    if old not in source:
        raise RuntimeError("generated mutation anchor changed")
    try:
        generated.write_text(source.replace(old, "world_1"))
        subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/I" + str(output),
                        str(ROOT / "tools/validation/shadow_reprojection_test.cpp"), "/Fe:shadow_reprojection_mutation.exe"],
                       cwd=output, check=True)
        result = subprocess.run([str(output / "shadow_reprojection_mutation.exe")], capture_output=True)
        if result.returncode == 0:
            raise RuntimeError("old point-distance comparison unexpectedly passed")
    finally:
        generated.write_text(source)
    print("shadow_reprojection_mutation=rejected targets=SPIR-V,DXIL,Metal gpu_runtime=not_run")


if __name__ == "__main__":
    main()
