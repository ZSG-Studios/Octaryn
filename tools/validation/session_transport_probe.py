"""Exercise real authority ingress and native mailbox I/O without a GPU."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "build"))
import vsenv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", default="release-windows")
    args = parser.parse_args()
    evidence_root = ROOT / "logs" / "server"
    evidence_root.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="session-transport-", dir=evidence_root))
    build = ROOT / "build" / args.preset
    source = ROOT / "tools" / "validation" / "SessionTransportProbe"
    native = build / "tools" / "SessionTransportProbe" / "native"
    native.mkdir(parents=True, exist_ok=True)
    with (evidence / "build.log").open("w", encoding="utf-8") as log:
        subprocess.run(["dotnet", "build", str(source / "SessionTransportProbe.csproj"),
                        "-c", "Release", f"-p:OctarynBuildPresetName={args.preset}", "--nologo"],
                       cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
        vs_root = vsenv.find_vs_root()
        vsenv.import_vs_environment(vs_root, "x64")
        vsenv.prepend_tool_dirs(ROOT, vs_root, "x64")
        owner = ROOT / "octaryn-client" / "Source" / "App" / "LocalSession"
        command = [vsenv.resolve_tool("clang-cl"), "/std:c++latest", "/EHsc", "/DNOMINMAX", "/O1",
                   "/I" + str(owner), "/I" + str(ROOT / "build/dependencies/src/glaze/include"),
                   str(source / "SessionIoProbe.cpp"), str(owner / "SessionIo.cpp"), str(owner / "SessionFiles.cpp"),
                   "/Fe:" + str(native / "SessionIoProbe.exe")]
        subprocess.run(command, cwd=native, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    with (evidence / "run.log").open("w", encoding="utf-8") as log:
        for command in [
            ["dotnet", str(build / "tools/SessionTransportProbe/managed/SessionTransportProbe.dll"),
             str(build / "server/bundle"), str(evidence / "authority")],
            [str(native / "SessionIoProbe.exe"), str(evidence / "client")],
        ]:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
    print((evidence / "run.log").read_text(encoding="utf-8"))
    print("session_transport_probe=passed evidence=" + str(evidence))


if __name__ == "__main__":
    main()

