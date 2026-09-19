"""Exercise production DDGI source publication and scheduling without GPU work."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / "build/release-windows/tools/ddgi-light-publication"
    output.mkdir(parents=True, exist_ok=True)
    backend = ROOT / "octaryn-client/Source/Rendering/RenderBackend"
    hook = (backend / "DDGILightingChanges.h").read_text()
    hook = hook.replace('#include "WorldRendererInternal.h"', '')
    (output / "DDGILightingChangesUnderTest.h").write_text(hook)
    vs = find_vs_root()
    import_vs_environment(vs, "x64")
    prepend_tool_dirs(ROOT, vs, "x64")
    includes = [output, backend, ROOT / "octaryn-client/Source/Rendering/Sky",
                ROOT / "octaryn-client/Source/Settings/LightingSettings",
                ROOT / "build/dependencies/slang-rhi/include",
                ROOT / "build/dependencies/slang-rhi-windows-x64-Release/include",
                ROOT / "build/dependencies/slang-2026.17.1/include"]
    command = ["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20"]
    command += ["/I" + str(path) for path in includes]
    log = ROOT / "logs/tools/ddgi-light-publication.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w") as stream:
        for test in ("ddgi_light_publication", "ddgi_schedule", "ddgi_schedule_response"):
            executable = output / f"{test}_test.exe"
            build = command + [str(ROOT / f"tools/validation/{test}_test.cpp"),
                               str(backend / "DDGISchedule.cpp"), f"/Fe:{executable}"]
            for args in (build, [str(executable)]):
                result = subprocess.run(args, cwd=output, text=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT)
                print(result.stdout, end="")
                stream.write(result.stdout)
                if result.returncode:
                    return result.returncode
        # The former soft-only publication must fail this same production fixture.
        anchor = 's.config.spacing,true,removed,true);'
        if hook.count(anchor) != 1:
            raise RuntimeError("source publication mutation anchor changed")
        generated = output / "DDGILightingChangesUnderTest.h"
        try:
            generated.write_text(hook.replace(anchor, 's.config.spacing,true,removed);'))
            executable = output / "ddgi_light_publication_old.exe"
            build = command + [str(ROOT / "tools/validation/ddgi_light_publication_test.cpp"),
                               str(backend / "DDGISchedule.cpp"), f"/Fe:{executable}"]
            subprocess.run(build, cwd=output, check=True)
            result = subprocess.run([str(executable)], cwd=output, text=True, capture_output=True)
            if result.returncode == 0 or "new source retained mature irradiance history" not in result.stderr:
                raise RuntimeError(f"soft-only publication was not rejected correctly: {result.stderr}")
            message = "ddgi_light_publication_old_soft_only=rejected\n"
            print(message, end="")
            stream.write(message + result.stderr)
        finally:
            generated.write_text(hook)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
