#!/usr/bin/env python3
import argparse
import pathlib
import sys


CONTENT_DIRS = ("Data", "Assets", "Shaders")
ALLOWED_DATA_SUFFIXES = {".json", ".toml", ".yaml", ".yml", ".txt", ".csv"}
ALLOWED_ASSET_SUFFIXES = {".json", ".png", ".jpg", ".jpeg", ".ktx2", ".glb", ".gltf", ".ogg", ".wav", ".txt",
                          ".rml", ".rcss", ".ttf"}
ALLOWED_SHADER_SUFFIXES = {".slang", ".hlsl", ".inc", ".wgsl", ".json", ".txt"}


def file_id(root, path):
    return path.relative_to(root).as_posix().lower()


def validate_files(root, subdir, allowed_suffixes):
    errors = []
    seen = set()
    directory = root / subdir
    if not directory.exists():
        return errors

    for path in directory.rglob("*"):
        if not path.is_file() or path.name == ".gitkeep":
            continue
        identifier = file_id(directory, path)
        if identifier in seen:
            errors.append(f"duplicate {subdir} entry {identifier}")
        seen.add(identifier)
        if path.suffix.lower() not in allowed_suffixes:
            errors.append(f"{path}: unsupported {subdir} file suffix {path.suffix}")
        if path.stat().st_size == 0:
            errors.append(f"{path}: empty {subdir} file")

    return errors


def validate_module_layout(module_root):
    errors = []
    for subdir in CONTENT_DIRS:
        if not (module_root / subdir).exists():
            errors.append(f"{module_root}: missing {subdir}/ directory")

    errors.extend(validate_files(module_root, "Data", ALLOWED_DATA_SUFFIXES))
    errors.extend(validate_files(module_root, "Assets", ALLOWED_ASSET_SUFFIXES))
    errors.extend(validate_files(module_root, "Shaders", ALLOWED_SHADER_SUFFIXES))
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--module-root", required=True)
    args = parser.parse_args()

    errors = validate_module_layout(pathlib.Path(args.module_root))
    if errors:
        for error in errors:
            print(f"module layout policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
