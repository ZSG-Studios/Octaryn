"""Run the production SessionIo worker probe in a fresh isolated directory."""
import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: validate_session_io.py <probe> <scratch-root>")
    executable = pathlib.Path(sys.argv[1]).resolve(strict=True)
    scratch = pathlib.Path(sys.argv[2]).resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="run-", dir=scratch) as directory:
        return subprocess.run([str(executable), str(pathlib.Path(directory) / "world")],
                              check=False, timeout=30).returncode


if __name__ == "__main__":
    raise SystemExit(main())
