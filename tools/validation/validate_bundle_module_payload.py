#!/usr/bin/env python3
import argparse
import json
import pathlib
import sys


def normalize_manifest(manifest):
    return json.dumps(manifest, sort_keys=True, separators=(",", ":"))


def validate(bundle_root, module_id, expected_manifest_path=None):
    descriptor_path = bundle_root / "Data" / "Module" / f"{module_id}.module.json"
    errors = []
    if not descriptor_path.exists():
        return [f"{descriptor_path}: module descriptor is missing"]

    descriptor_dir = descriptor_path.parent
    descriptors = sorted(descriptor_dir.glob("*.module.json")) if descriptor_dir.exists() else []
    if descriptors != [descriptor_path]:
        errors.append(
            f"{descriptor_dir}: expected only {descriptor_path.name}, found {[path.name for path in descriptors]}")

    manifest = json.loads(descriptor_path.read_text(encoding="utf-8"))
    if manifest.get("ModuleId") != module_id:
        errors.append(f"{descriptor_path}: ModuleId {manifest.get('ModuleId')!r} does not match {module_id!r}")

    if expected_manifest_path is not None:
        expected_manifest = json.loads(expected_manifest_path.read_text(encoding="utf-8"))
        if normalize_manifest(manifest) != normalize_manifest(expected_manifest):
            errors.append(f"{descriptor_path}: bundled module descriptor does not match {expected_manifest_path}")
    elif not manifest.get("ContentDeclarations"):
        errors.append(f"{descriptor_path}: module descriptor must declare content")

    declared_payloads = set()
    for declaration in manifest.get("ContentDeclarations", []):
        validate_declared_path(errors, bundle_root, declared_payloads, declaration, "ContentId", "RelativePath")
    for declaration in manifest.get("AssetDeclarations", []):
        validate_declared_path(errors, bundle_root, declared_payloads, declaration, "AssetId", "RelativePath")

    validate_no_undeclared_payloads(errors, bundle_root, declared_payloads)

    return errors


def validate_declared_path(errors, bundle_root, declared_payloads, declaration, id_key, path_key):
    declaration_id = declaration.get(id_key, "<missing id>")
    relative_path = declaration.get(path_key)
    if not isinstance(relative_path, str) or not relative_path:
        errors.append(f"{declaration_id}: missing {path_key}")
        return
    if relative_path.startswith(("/", "\\")) or ".." in relative_path or ":" in relative_path:
        errors.append(f"{declaration_id}: unsafe bundle payload path: {relative_path}")
        return

    declared_payloads.add(relative_path)
    path = (bundle_root / relative_path).resolve()
    if bundle_root not in path.parents:
        errors.append(f"{declaration_id}: bundle payload escapes bundle root: {relative_path}")
        return
    if not path.exists():
        errors.append(f"{declaration_id}: bundle payload is missing: {path}")
    elif path.stat().st_size == 0:
        errors.append(f"{path}: bundle payload is empty")


def validate_no_undeclared_payloads(errors, bundle_root, declared_payloads):
    for root_name in ("Data", "Assets", "Shaders"):
        root = bundle_root / root_name
        if not root.exists():
            continue

        for path in sorted(root.rglob("*")):
            if not path.is_file() or path.name == ".gitkeep":
                continue

            relative = path.relative_to(bundle_root).as_posix()
            if relative.startswith("Data/Module/") and relative.endswith(".module.json"):
                continue
            if relative not in declared_payloads:
                errors.append(f"{path}: bundle payload is not declared by module descriptor")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle-root", required=True)
    parser.add_argument("--module-id", required=True)
    parser.add_argument("--expected-manifest")
    args = parser.parse_args()

    expected_manifest = pathlib.Path(args.expected_manifest).resolve() if args.expected_manifest else None
    errors = validate(pathlib.Path(args.bundle_root).resolve(), args.module_id, expected_manifest)
    if errors:
        for error in errors:
            print(f"bundle module payload policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
