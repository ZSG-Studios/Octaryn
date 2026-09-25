#!/usr/bin/env python3
import argparse
import json
import pathlib
import re
import sys


CONTENT_DECLARATION = re.compile(
    r"new\s+GameModuleContentDeclaration\s*\(\s*\"([^\"]+)\"\s*,\s*\"([^\"]+)\"\s*,\s*\"([^\"]+)\"\s*\)",
    re.DOTALL)
ASSET_DECLARATION = re.compile(
    r"new\s+GameModuleAssetDeclaration\s*\(\s*\"([^\"]+)\"\s*,\s*\"([^\"]+)\"\s*,\s*\"([^\"]+)\"\s*\)",
    re.DOTALL)
IGNORABLE_BLOCK_TEXT = re.compile(r"[\s,]*")
MODULE_DESCRIPTOR_PATTERN = re.compile(r"^Data/Module/[^/]+\.module\.json$")
PLACEHOLDER_TEXT = re.compile(r"\bplaceholder\b", re.IGNORECASE)

def source_files(module_root):
    yield from sorted((module_root / "Source").rglob("*.cs"))


def find_bracketed_argument(text, argument_name):
    anchor = re.search(rf"\b{re.escape(argument_name)}\s*:\s*\[", text)
    if not anchor:
        return None

    start = anchor.end()
    depth = 1
    index = start
    while index < len(text):
        char = text[index]
        if char == "[":
            depth += 1
        elif char == "]":
            depth -= 1
            if depth == 0:
                return text[start:index]
        index += 1

    return None


def parse_manifest_declarations(module_root):
    content = []
    assets = []
    manifest_files = []
    errors = []

    for path in source_files(module_root):
        text = path.read_text(encoding="utf-8")
        content_block = find_bracketed_argument(text, "ContentDeclarations")
        asset_block = find_bracketed_argument(text, "AssetDeclarations")
        if content_block is None and asset_block is None:
            continue
        manifest_files.append(path)
        if content_block is not None:
            content.extend(CONTENT_DECLARATION.findall(content_block))
            errors.extend(validate_supported_block(path, "ContentDeclarations", content_block, CONTENT_DECLARATION))
        if asset_block is not None:
            assets.extend(ASSET_DECLARATION.findall(asset_block))
            errors.extend(validate_supported_block(path, "AssetDeclarations", asset_block, ASSET_DECLARATION))

    if len(manifest_files) > 1:
        errors.append(
            f"{module_root}: manifest declarations must live in one source file, found "
            f"{[path.as_posix() for path in manifest_files]}")

    return bool(manifest_files), content, assets, errors


def parse_manifest_json(path):
    manifest = json.loads(path.read_text(encoding="utf-8"))
    content = [
        (
            declaration["ContentId"],
            declaration["ContentKind"],
            declaration["RelativePath"],
        )
        for declaration in manifest.get("ContentDeclarations", [])
    ]
    assets = [
        (
            declaration["AssetId"],
            declaration["AssetKind"],
            declaration["RelativePath"],
        )
        for declaration in manifest.get("AssetDeclarations", [])
    ]
    return True, content, assets, []


def validate_supported_block(path, block_name, block, declaration_pattern):
    remainder = declaration_pattern.sub("", block)
    if IGNORABLE_BLOCK_TEXT.fullmatch(remainder):
        return []

    return [
        f"{path}: {block_name} contains unsupported declaration syntax; "
        "use explicit new GameModuleContentDeclaration(...) or new GameModuleAssetDeclaration(...) records with literal IDs, kinds, and paths"
    ]


def validate_declared_content_file(path, content_id, content_kind):
    if path.suffix.lower() != ".json":
        return []

    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as error:
        return [f"{path}: declared JSON content file must be UTF-8 text: {error}"]

    errors = []
    try:
        document = json.loads(text)
    except json.JSONDecodeError as error:
        errors.append(f"{path}: declared JSON content file is invalid: {error}")
        document = None

    if isinstance(document, dict):
        declared_id = document.get("id")
        declared_kind = document.get("kind")
        if declared_id != content_id:
            errors.append(
                f"{path}: declared JSON content id {declared_id!r} does not match manifest content id {content_id!r}")
        if declared_kind != content_kind:
            errors.append(
                f"{path}: declared JSON content kind {declared_kind!r} does not match manifest content kind {content_kind!r}")
    elif document is not None:
        errors.append(f"{path}: declared JSON content file must be an object with id and kind fields")

    if PLACEHOLDER_TEXT.search(text):
        errors.append(f"{path}: declared content file still contains placeholder text")

    return errors


def validate(module_root, manifest_json=None):
    module_root = module_root.resolve()
    if manifest_json is None:
        found_manifest, content, assets, errors = parse_manifest_declarations(module_root)
    else:
        found_source, source_content, source_assets, errors = parse_manifest_declarations(module_root)
        found_manifest, content, assets, json_errors = parse_manifest_json(manifest_json)
        errors.extend(json_errors)
        if not found_source:
            errors.append(f"{module_root}: no source content/asset manifest declarations found under Source/")
        elif set(source_content) != set(content) or set(source_assets) != set(assets):
            errors.append(
                "generated manifest content/asset declarations do not match source declarations: "
                f"source content {sorted(source_content)}, generated content {sorted(content)}, "
                f"source assets {sorted(source_assets)}, generated assets {sorted(assets)}")

    if not found_manifest:
        return [f"{module_root}: no content/asset manifest declarations found under Source/"]

    if not content:
        errors.append(f"{module_root}: module manifest must declare at least one content record")
    declared_content_paths = set()
    declared_asset_paths = set()

    for content_id, content_kind, relative_path in content:
        if relative_path.startswith(("/", "\\")) or ".." in relative_path or ":" in relative_path:
            errors.append(f"{content_id}: content path is not a safe relative path: {relative_path}")
            continue
        if not relative_path.startswith("Data/"):
            errors.append(f"{content_id}: content path must be under Data/: {relative_path}")
            continue
        declared_content_paths.add(relative_path)
        path = (module_root / relative_path).resolve()
        if module_root not in path.parents:
            errors.append(f"{content_id}: declared content path escapes module root: {relative_path}")
            continue
        if not path.exists():
            errors.append(f"{content_id}: declared content file is missing: {path}")
        elif path.stat().st_size == 0:
            errors.append(f"{path}: declared content file is empty")
        else:
            errors.extend(validate_declared_content_file(path, content_id, content_kind))

    for asset_id, _asset_kind, relative_path in assets:
        if relative_path.startswith(("/", "\\")) or ".." in relative_path or ":" in relative_path:
            errors.append(f"{asset_id}: asset path is not a safe relative path: {relative_path}")
            continue
        if not (relative_path.startswith("Assets/") or relative_path.startswith("Shaders/")):
            errors.append(f"{asset_id}: asset path must be under Assets/ or Shaders/: {relative_path}")
            continue
        declared_asset_paths.add(relative_path)
        path = (module_root / relative_path).resolve()
        if module_root not in path.parents:
            errors.append(f"{asset_id}: declared asset path escapes module root: {relative_path}")
            continue
        if not path.exists():
            errors.append(f"{asset_id}: declared asset file is missing: {path}")
        elif path.stat().st_size == 0:
            errors.append(f"{path}: declared asset file is empty")

    for path in (module_root / "Data").rglob("*"):
        if path.is_file() and path.name != ".gitkeep":
            relative = path.relative_to(module_root).as_posix()
            if MODULE_DESCRIPTOR_PATTERN.fullmatch(relative):
                continue
            if relative not in declared_content_paths:
                errors.append(f"{path}: data file is not declared by module manifest")

    for root_name in ("Assets", "Shaders"):
        for path in (module_root / root_name).rglob("*"):
            if path.is_file() and path.name != ".gitkeep":
                relative = path.relative_to(module_root).as_posix()
                if relative not in declared_asset_paths:
                    errors.append(f"{path}: asset/shader file is not declared by module manifest")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--module-root", required=True)
    parser.add_argument("--manifest-json")
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.module_root),
        pathlib.Path(args.manifest_json) if args.manifest_json else None)
    if errors:
        for error in errors:
            print(f"module manifest file policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
