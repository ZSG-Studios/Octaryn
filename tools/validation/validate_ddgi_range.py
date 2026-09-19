"""CPU-only production DDGI range bounds, memory accounting and scheduling benchmark."""
from pathlib import Path
import argparse
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-baseline", action="store_true")
    args = parser.parse_args()
    backend = ROOT / "octaryn-client/Source/Rendering/RenderBackend"
    output = ROOT / "build/release-windows/tools/ddgi-range"
    output.mkdir(parents=True, exist_ok=True)
    stage = "before" if args.capture_baseline else "after"
    source = output / stage
    source.mkdir(exist_ok=True)
    for name in ("DDGIVolumeConfig.h", "DDGISystem.h", "DDGITiming.h", "DDGISchedule.cpp", "LightingQuality.h"):
        shutil.copy2(backend / name, source / name)
    vs = find_vs_root()
    import_vs_environment(vs, "x64")
    prepend_tool_dirs(ROOT, vs, "x64")
    includes = [source, ROOT / "build/dependencies/slang-rhi/include",
                ROOT / "build/dependencies/slang-rhi-windows-x64-Release/include",
                ROOT / "build/dependencies/slang-2026.17.1/include"]
    executable = source / "ddgi_range_test.exe"
    command = ["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20"]
    command += ["/I" + str(path) for path in includes]
    command += [str(ROOT / "tools/validation/ddgi_range_test.cpp"),
                str(source / "DDGISchedule.cpp"), f"/Fe:{executable}"]
    run = [str(executable)] + (["benchmark"] if args.capture_baseline else [])
    log = ROOT / f"logs/tools/ddgi-range-{stage}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w") as stream:
        for cmd in (command, run):
            result = subprocess.run(cmd, cwd=source, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            print(result.stdout, end="")
            stream.write(result.stdout)
            if result.returncode:
                return result.returncode
        if not args.capture_baseline:
            production = (backend / "DDGISystem.cpp").read_text()
            start = production.index("bool world_ddgi_reconfigure(WorldRenderer& r) {")
            end = production.index("bool world_ddgi_bind(", start)
            lighting = (backend / "LightingSystem.cpp").read_text()
            apply_start = lighting.index("void apply_quality(WorldRenderer& r) {")
            apply_end = lighting.index("\n}\n}", apply_start) + 2
            quality_start = lighting.index("void open_world_renderer_set_lighting_quality(")
            quality_end = lighting.index("void open_world_renderer_set_raster_shadows(", quality_start)
            (source / "DDGIReconfigureUnderTest.h").write_text(
                "namespace octaryn::client::rendering {\n" + lighting[apply_start:apply_end] + "\n" +
                lighting[quality_start:quality_end] + production[start:end] + "}\n")
            executable = source / "ddgi_reconfigure_test.exe"
            build = command[:-3] + [str(ROOT / "tools/validation/ddgi_reconfigure_test.cpp"), f"/Fe:{executable}"]
            for cmd in (build, [str(executable)]):
                result = subprocess.run(cmd, cwd=source, text=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT)
                print(result.stdout, end="")
                stream.write(result.stdout)
                if result.returncode:
                    return result.returncode
            executable = source / "ddgi_quality_test.exe"
            build = command[:-3] + [str(ROOT / "tools/validation/ddgi_quality_test.cpp"),
                                    str(source / "DDGISchedule.cpp"), f"/Fe:{executable}"]
            for cmd in (build, [str(executable)]):
                result = subprocess.run(cmd, cwd=source, text=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT)
                print(result.stdout, end="")
                stream.write(result.stdout)
                if result.returncode:
                    return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
