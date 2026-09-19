#!/usr/bin/env python3
"""Compile and execute the production lighting-source change detector fixture."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    windows = os.name == "nt"
    if windows:
        sys.path.insert(0, str(ROOT / "tools/build"))
        import vsenv
        vsenv.import_vs_environment(vsenv.find_vs_root(), "x64")
    preset = "release-windows" if windows else "release-linux"
    output = ROOT / "build" / preset / "tools/lighting-changes"
    output.mkdir(parents=True, exist_ok=True)
    binary = output / ("lighting_changes_test.exe" if windows else "lighting_changes_test")
    includes = [ROOT / "octaryn-client/Source/Rendering/RenderBackend",
                ROOT / "octaryn-client/Source/Rendering/Sky",
                ROOT / "octaryn-client/Source/Settings/LightingSettings"]
    sources = [ROOT / "tools/validation/lighting_changes_test.cpp",
               ROOT / "octaryn-client/Source/Rendering/Sky/SkyData.cpp"]
    command = (["cl", "/nologo", "/std:c++20", "/EHsc", "/W4", f"/Fe:{binary}"]
               if windows else ["clang++", "-std=c++20", "-Wall", "-Wextra", "-o", str(binary)])
    command += [("/I" if windows else "-I") + str(path) for path in includes]
    command += [str(path) for path in sources]
    log = ROOT / "logs/tools/lighting-changes.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as stream:
        for args in [command, [str(binary)]]:
            result = subprocess.run(args, cwd=output, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            print(result.stdout, end="")
            stream.write(result.stdout)
            if result.returncode:
                return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
