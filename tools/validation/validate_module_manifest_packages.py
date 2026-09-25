#!/usr/bin/env python3
import argparse
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET


RUNTIME_BLOCK = re.compile(r"RequestedRuntimePackages:\s*\[(.*?)\]", re.DOTALL)
BUILD_BLOCK = re.compile(r"RequestedBuildPackages:\s*\[(.*?)\]", re.DOTALL)
ALLOWED_PACKAGE_CONSTANT = re.compile(r"AllowedPackageIds\.([A-Za-z0-9_]+)")
CONSTANT_DECLARATION = re.compile(r"public\s+const\s+string\s+([A-Za-z0-9_]+)\s*=\s*\"([^\"]+)\"")
IGNORABLE_BLOCK_TEXT = re.compile(r"[\s,]*")


def load_allowed_package_ids(path):
    text = path.read_text(encoding="utf-8")
    return dict(CONSTANT_DECLARATION.findall(text))


def package_references(project_file):
    tree = ET.parse(project_file)
    root = tree.getroot()
    runtime = set()
    build = set()
    for item in root.findall(".//PackageReference"):
        package = item.attrib.get("Include")
        if not package:
            continue
        private_assets = item.attrib.get("PrivateAssets", "")
        output_item_type = item.attrib.get("OutputItemType", "")
        include_assets = item.attrib.get("IncludeAssets", "")
        if private_assets == "all" and output_item_type == "Analyzer" and "analyzers" in include_assets:
            build.add(package)
        else:
            runtime.add(package)
    return runtime, build


def manifest_packages(module_root, constants):
    source_files = sorted((module_root / "Source").rglob("*.cs"))
    runtime = set()
    build = set()
    manifest_files = []
    errors = []
    for path in source_files:
        text = path.read_text(encoding="utf-8")
        runtime_match = RUNTIME_BLOCK.search(text)
        build_match = BUILD_BLOCK.search(text)
        if not runtime_match and not build_match:
            continue
        manifest_files.append(path)
        if runtime_match:
            runtime_block = runtime_match.group(1)
            runtime.update(resolve_packages(runtime_block, constants, path))
            errors.extend(validate_supported_block(path, "RequestedRuntimePackages", runtime_block))
        if build_match:
            build_block = build_match.group(1)
            build.update(resolve_packages(build_block, constants, path))
            errors.extend(validate_supported_block(path, "RequestedBuildPackages", build_block))

    if len(manifest_files) > 1:
        errors.append(
            f"{module_root}: manifest package declarations must live in one source file, found "
            f"{[path.as_posix() for path in manifest_files]}")

    return bool(manifest_files), runtime, build, errors


def manifest_packages_json(path):
    manifest = json.loads(path.read_text(encoding="utf-8"))
    return (
        True,
        set(manifest.get("RequestedRuntimePackages", [])),
        set(manifest.get("RequestedBuildPackages", [])),
        [],
    )


def resolve_packages(block, constants, path):
    packages = set()
    for constant in ALLOWED_PACKAGE_CONSTANT.findall(block):
        package = constants.get(constant)
        if package is None:
            raise ValueError(f"{path}: unknown AllowedPackageIds constant {constant}")
        packages.add(package)
    return packages


def validate_supported_block(path, block_name, block):
    remainder = ALLOWED_PACKAGE_CONSTANT.sub("", block)
    if IGNORABLE_BLOCK_TEXT.fullmatch(remainder):
        return []

    return [
        f"{path}: {block_name} contains unsupported package syntax; "
        "use AllowedPackageIds constants directly so pre-build policy can verify the manifest"
    ]


def validate(module_root, project_file, allowed_package_ids_file, manifest_json=None):
    constants = load_allowed_package_ids(allowed_package_ids_file)
    project_runtime, project_build = package_references(project_file)
    if manifest_json is None:
        found_manifest, manifest_runtime, manifest_build, errors = manifest_packages(module_root, constants)
    else:
        found_source, source_runtime, source_build, errors = manifest_packages(module_root, constants)
        found_manifest, manifest_runtime, manifest_build, json_errors = manifest_packages_json(manifest_json)
        errors.extend(json_errors)
        if not found_source:
            errors.append(f"{module_root}: no source manifest package declarations found under Source/")
        elif source_runtime != manifest_runtime or source_build != manifest_build:
            errors.append(
                "generated manifest package declarations do not match source declarations: "
                f"source runtime {sorted(source_runtime)}, generated runtime {sorted(manifest_runtime)}, "
                f"source build {sorted(source_build)}, generated build {sorted(manifest_build)}")

    if not found_manifest:
        errors.append(f"{module_root}: no manifest package declarations found under Source/")
        return errors

    if project_runtime != manifest_runtime:
        errors.append(
            "runtime PackageReference set does not match manifest RequestedRuntimePackages: "
            f"project {sorted(project_runtime)}, manifest {sorted(manifest_runtime)}")

    if project_build != manifest_build:
        errors.append(
            "build/analyzer PackageReference set does not match manifest RequestedBuildPackages: "
            f"project {sorted(project_build)}, manifest {sorted(manifest_build)}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--module-root", required=True)
    parser.add_argument("--project-file", required=True)
    parser.add_argument(
        "--allowed-package-ids-file",
        default="octaryn-shared/Source/FrameworkAllowlist/AllowedPackageIds.cs")
    parser.add_argument("--manifest-json")
    args = parser.parse_args()

    try:
        errors = validate(
            pathlib.Path(args.module_root),
            pathlib.Path(args.project_file),
            pathlib.Path(args.allowed_package_ids_file),
            pathlib.Path(args.manifest_json) if args.manifest_json else None)
    except ValueError as error:
        errors = [str(error)]

    if errors:
        for error in errors:
            print(f"module manifest package policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
