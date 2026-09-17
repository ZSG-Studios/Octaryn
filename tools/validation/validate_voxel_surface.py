"""Compile portable surface queries and execute their generated Slang CPU code."""
import subprocess
from validate_voxel_dda import ROOT, compile_shader


def run():
    compile_shader(ROOT / "tools/validation/VoxelSurfaceProbe.slang", "Surface")
    output = ROOT / "build/release-windows/tools/voxel-dda"
    subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/I"+str(output),
                    str(ROOT / "tools/validation/voxel_surface_test.cpp"), "/Fe:voxel_surface_test.exe"],
                   cwd=output, check=True)
    subprocess.run([str(output / "voxel_surface_test.exe")], cwd=ROOT, check=True)


if __name__ == "__main__":
    run()
