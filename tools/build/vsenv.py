"""Shared Visual Studio developer-environment bootstrap for Windows builds.

Both Windows entrypoints (windows.py and slang-rhi.py) import this instead of
duplicating VsDevShell handling. clang-cl needs the VsDevShell environment for
MSVC and Windows SDK headers; standalone LLVM on PATH is not enough.
"""
import os
from pathlib import Path
import shutil
import subprocess


def find_vs_root():
    program_files_x86 = os.environ.get("ProgramFiles(x86)")
    vswhere = Path(program_files_x86) / "Microsoft Visual Studio/Installer/vswhere.exe" \
        if program_files_x86 else None
    if not vswhere or not vswhere.is_file():
        raise ValueError("Install Visual Studio C++ build tools (vswhere not found)")
    root = subprocess.run(
        [str(vswhere), "-latest", "-products", "*",
         "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         "-property", "installationPath"],
        check=True, text=True, stdout=subprocess.PIPE).stdout.strip()
    if not root:
        raise ValueError("Visual Studio C++ build tools were not found")
    return Path(root)


def import_vs_environment(vs_root, arch):
    """Capture the VsDevShell environment for arch (x64 or arm64) into this process."""
    launcher = Path(vs_root) / "Common7/Tools/Launch-VsDevShell.ps1"
    script = (f"& '{launcher}' -Arch {arch} -HostArch amd64 -SkipAutomaticLocation; "
              "Get-ChildItem Env: | ForEach-Object { $_.Name + '=' + $_.Value }")
    output = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script],
        check=True, text=True, stdout=subprocess.PIPE).stdout
    environment = {}
    for line in output.splitlines():
        name, separator, value = line.partition("=")
        if separator and name:
            environment[name] = value
    if environment.get("VSCMD_ARG_TGT_ARCH") != arch:
        raise ValueError("Visual Studio target environment was not initialized")
    os.environ.update(environment)


def prepend_tool_dirs(repo_root, vs_root, arch):
    """Put repo-pinned cmake/ninja, VS LLVM and standalone LLVM first on PATH."""
    repo_root, vs_root = Path(repo_root), Path(vs_root)
    tool_dirs = [
        repo_root / "build/dependencies/tools/cmake/bin",
        repo_root / "build/dependencies/tools/ninja",
        vs_root / f"VC/Tools/Llvm/{arch}/bin",
        Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "LLVM/bin",
    ]
    os.environ["PATH"] = os.pathsep.join([str(d) for d in tool_dirs] + [os.environ["PATH"]])


def require_tools(*names, hint="see docs/build/README.md"):
    for tool in names:
        if not shutil.which(tool):
            raise ValueError(f"Missing required tool: {tool}; {hint}")


def resolve_tool(name):
    """Absolute path of a tool after PATH setup; failure to resolve is fatal."""
    path = shutil.which(name)
    if not path:
        raise ValueError(f"Missing required tool: {name}; see docs/build/README.md")
    return path
