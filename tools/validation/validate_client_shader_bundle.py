#!/usr/bin/env python3
import argparse
import filecmp
import pathlib
import sys


COMPILED_SHADER_STEMS = (
    "composite.comp",
    "opaque_packed.vert",
    "opaque.frag",
    "player_model.frag",
    "player_model.vert",
    "present.frag",
    "present.vert",
    "sky.frag",
    "sky.vert",
    "sprite_packed.vert",
    "ui.comp",
)

EXPECTED_COMPILED_SHADERS = {
    f"{stem}{suffix}"
    for stem in COMPILED_SHADER_STEMS
    for suffix in (".json", ".msl", ".spv", ".spvcross.json")
}


def relative_files(root):
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file() and path.name != ".gitkeep"
    }


def validate(source_root, bundle_shader_root):
    errors = []

    if not source_root.exists():
        errors.append(f"{source_root}: client shader source root is missing")
    if not bundle_shader_root.exists():
        errors.append(f"{bundle_shader_root}: client bundle shader root is missing")
    if errors:
        return errors

    source_files = relative_files(source_root)
    bundled_files = relative_files(bundle_shader_root)
    bundled_source_files = {
        path for path in bundled_files if not path.startswith("Compiled/")
    }
    compiled_files = {
        path.removeprefix("Compiled/")
        for path in bundled_files
        if path.startswith("Compiled/")
    }
    if not source_files:
        errors.append(f"{source_root}: client shader source root has no shader files")

    missing = sorted(source_files - bundled_source_files)
    extra = sorted(bundled_source_files - source_files)
    if missing:
        errors.append(f"{bundle_shader_root}: missing client-owned shader files: {missing}")
    if extra:
        errors.append(f"{bundle_shader_root}: contains undeclared client shader files: {extra}")

    if compiled_files:
        missing_compiled = sorted(EXPECTED_COMPILED_SHADERS - compiled_files)
        extra_compiled = sorted(compiled_files - EXPECTED_COMPILED_SHADERS)
        if missing_compiled:
            errors.append(f"{bundle_shader_root / 'Compiled'}: missing compiled runtime shaders: {missing_compiled}")
        if extra_compiled:
            errors.append(f"{bundle_shader_root / 'Compiled'}: contains undeclared compiled runtime shaders: {extra_compiled}")

    for relative in sorted(source_files & bundled_source_files):
        source_file = source_root / relative
        bundled_file = bundle_shader_root / relative
        if not filecmp.cmp(source_file, bundled_file, shallow=False):
            errors.append(f"{bundled_file}: does not match client-owned source {source_file}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--bundle-shader-root", required=True)
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.source_root).resolve(),
        pathlib.Path(args.bundle_shader_root).resolve())
    if errors:
        for error in errors:
            print(f"client shader bundle policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
