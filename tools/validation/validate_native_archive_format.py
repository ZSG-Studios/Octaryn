#!/usr/bin/env python3
import argparse
import pathlib
import subprocess
import sys


EXPECTED_FORMATS = {
    "elf64-x86-64": {
        "arguments": ("-f",),
        "tokens": ("elf64-x86-64",),
        "tool_name": "objdump",
    },
    "elf64-aarch64": {
        "arguments": ("-f",),
        "tokens": ("elf64-littleaarch64", "architecture: aarch64"),
        "tool_name": "objdump",
    },
    "pe-x86-64": {
        "arguments": ("-f",),
        "tokens": ("coff-x86-64",),
        "tool_name": "objdump",
    },
    "coff-arm64": {
        "arguments": ("--file-headers",),
        "tokens": ("COFF-ARM64", "Arch: aarch64", "IMAGE_FILE_MACHINE_ARM64"),
        "tool_name": "llvm-readobj",
    },
}


def validate(archive, expected_format, objdump):
    if expected_format not in EXPECTED_FORMATS:
        return [f"unsupported expected archive format {expected_format}"]

    if not archive.exists():
        return [f"{archive}: native archive does not exist"]

    expected = EXPECTED_FORMATS[expected_format]
    result = subprocess.run(
        [objdump, *expected["arguments"], str(archive)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    if result.returncode != 0:
        return [f"{archive}: {expected['tool_name']} failed: {result.stderr.strip()}"]

    missing_tokens = [token for token in expected["tokens"] if token not in result.stdout]
    if missing_tokens:
        return [
            f"{archive}: expected object format tokens {', '.join(expected['tokens'])}, "
            f"{expected['tool_name']} output was {result.stdout.strip()}"
        ]

    return []


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", required=True)
    parser.add_argument("--expected-format", required=True)
    parser.add_argument("--objdump", default="objdump")
    args = parser.parse_args()

    errors = validate(pathlib.Path(args.archive), args.expected_format, args.objdump)
    if errors:
        for error in errors:
            print(f"native archive format policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
