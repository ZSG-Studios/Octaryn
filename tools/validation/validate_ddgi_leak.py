"""Execute production DDGI sampling and reject occlusion-normalization regressions."""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / "build/release-windows/tools/ddgi-leak"
    output.mkdir(parents=True, exist_ok=True)
    slang = ROOT / "build/dependencies/slang-2026.17.1/bin/slangc.exe"
    shaders = ROOT / "octaryn-client/Shaders"
    vs = find_vs_root()
    import_vs_environment(vs, "x64")
    prepend_tool_dirs(ROOT, vs, "x64")

    def run(include, expected_failure=None):
        subprocess.run([str(slang), str(ROOT / "tools/validation/DDGILeakProbe.slang"),
                        "-I", str(include), "-entry", "main", "-target", "cpp",
                        "-o", str(output / "DDGILeakCpu.cpp")], check=True)
        subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/I" + str(output),
                        str(ROOT / "tools/validation/ddgi_leak_test.cpp"), "/Fe:ddgi_leak_test.exe"],
                       cwd=output, check=True)
        result = subprocess.run([str(output / "ddgi_leak_test.exe")], capture_output=True, text=True)
        if expected_failure:
            if result.returncode == 0 or expected_failure not in result.stderr:
                raise RuntimeError(f"negative control failed incorrectly: {result.stdout} {result.stderr}")
            print(f"ddgi_leak_negative_control=rejected reason={expected_failure}")
        else:
            print(result.stdout, end="")
            if result.returncode:
                raise RuntimeError(f"exit={result.returncode}: {result.stderr}")

    run(shaders)
    mutant = output / "mutant-shaders"
    shutil.copytree(shaders, mutant, dirs_exist_ok=True)
    sample = mutant / "DDGI/DDGISample.slang"
    original = sample.read_text()
    mutations = (
        ("return max(result/total,0)*support;", "return max(result/total,0);",
         "all-blocked cage renormalized outdoor energy"),
        ("weight*=chebyshev;", "weight*=max(.05,chebyshev);",
         "blocked bright corners contaminated visible dark probe"),
    )
    for anchor, replacement, failure in mutations:
        if original.count(anchor) != 1:
            raise RuntimeError(f"mutation anchor changed: {anchor}")
        sample.write_text(original.replace(anchor, replacement))
        run(mutant, failure)
    # Leave the output executable/generated C++ representing production, not a mutant.
    run(shaders)
    entries = (("DDGI/DDGITrace.slang", "main", []), ("Hdr/CompositeRT.slang", "main", []),
               ("Voxel/WorldRaster.slang", "forward_main", ["-DOCTARYN_RAY_TRACING=1"]),
               (str(ROOT / "tools/validation/DDGILeakProbe.slang"), "main", []))
    for shader, entry, defines in entries:
        for target, suffix, flags in (
            ("spirv", "spv", ["-profile", "spirv_1_5"]),
            ("dxil", "dxil", ["-profile", "sm_6_6", "-dxc-path",
                              str(ROOT / "build/dependencies/slang-rhi-windows-x64-Release/_deps/dxc-src/bin/x64")]),
            ("metal", "metal", []),
        ):
            subprocess.run([str(slang), str(shaders / shader), "-I", str(shaders), "-entry", entry, "-target", target,
                            *flags, *defines, "-o", str(output / f"{Path(shader).stem}.{suffix}")], check=True)
    print("ddgi_leak_targets=SPIR-V,DXIL,Metal gpu_runtime=not_run")


if __name__ == "__main__":
    main()
