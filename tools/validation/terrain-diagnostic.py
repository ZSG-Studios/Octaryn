#!/usr/bin/env python3
"""Compile and run the standalone terrain diagnostic, then encode PNGs."""
import argparse
from pathlib import Path
import subprocess
import sys


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/build"))
import vsenv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-directory", default="logs/server/terrain-diagnostic")
    args = parser.parse_args()

    vs_root = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs_root, "x64")
    vsenv.prepend_tool_dirs(REPO, vs_root, "x64")
    vsenv.require_tools("clang-cl")

    binary_directory = REPO / "build/release-windows/tools/terrain-validation"
    binary_directory.mkdir(parents=True, exist_ok=True)
    binary = binary_directory / "octaryn_terrain_diagnostic.exe"
    source = REPO / "tools/Source/ServerTerrainGenerationProbe/TerrainDiagnostic.cpp"
    include = REPO / "octaryn-basegame/Source/Gameplay/Terrain"
    subprocess.run(["clang-cl", "/nologo", "/std:c++latest", "/EHsc", "/O2", "/MD", "/W4", "/WX",
                    f"/I{include}", f"/Fo{binary_directory / 'TerrainDiagnostic.obj'}",
                    f"/Fe{binary}", str(source)], check=True)

    output = Path(args.output_directory)
    if not output.is_absolute():
        output = REPO / output
    subprocess.run([str(binary), str(output)], check=True)
    subprocess.run([sys.executable, str(Path(__file__).parent / "terrain-diagnostic-images.py"),
                    str(output)], check=True)
    print(f"Terrain diagnostic artifacts: {output}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
