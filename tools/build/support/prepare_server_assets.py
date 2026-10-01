"""Keep module presentation resources out of the headless server package."""
import argparse
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    stage = parser.parse_args().stage.resolve()
    build = Path(__file__).resolve().parents[3] / "build"
    if not stage.is_relative_to(build.resolve()) or stage.name != "bundle.staging" or stage.parent.name != "server":
        raise ValueError("Expected a server staging directory inside the workspace build directory")
    for name in ("Ui", "Audio"):
        target = stage / "Assets" / name
        if target.is_symlink() or not target.resolve().is_relative_to(stage):
            raise ValueError("Presentation asset directory escapes the server stage")
        if target.is_dir():
            shutil.rmtree(target)
    print("server_assets=passed presentation_payloads=0")


if __name__ == "__main__":
    main()
