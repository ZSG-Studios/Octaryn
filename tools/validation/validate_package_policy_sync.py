#!/usr/bin/env python3
import argparse
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET


DEFAULT_POLICY = pathlib.Path(__file__).resolve().parents[1] / "package-policy" / "module-packages.json"
DEFAULT_PROPS = pathlib.Path(__file__).resolve().parents[1] / "package-policy" / "ModulePackagePolicy.props"
DEFAULT_DIRECTORY_PACKAGES = pathlib.Path(__file__).resolve().parents[2] / "Directory.Packages.props"
DEFAULT_ALLOWED_PACKAGE_IDS = pathlib.Path(__file__).resolve().parents[2] / "octaryn-shared" / "Source" / "FrameworkAllowlist" / "AllowedPackageIds.cs"
DEFAULT_MODULE_PACKAGE_ALLOWLIST = pathlib.Path(__file__).resolve().parents[2] / "octaryn-shared" / "Source" / "FrameworkAllowlist" / "ModulePackageAllowlist.cs"
DEFAULT_MODULE_BUILD_PACKAGE_ALLOWLIST = pathlib.Path(__file__).resolve().parents[2] / "octaryn-shared" / "Source" / "FrameworkAllowlist" / "ModuleBuildPackageAllowlist.cs"
PACKAGE_SECTIONS = ("runtimeDirect", "runtimeTransitive", "buildDirect", "buildTransitive", "hostDirect", "hostOnly")
METADATA_FIELDS = ("allowedOwners", "purpose", "versionPolicy", "runtimeScope", "validationRule", "enforcement")
KNOWN_OWNERS = ("basegame", "client", "game", "game-module", "mod", "server")


def load_json_policy(path):
    with open(path, "r", encoding="utf-8") as file:
        policy = json.load(file)
    return policy


def validate_policy_metadata(policy):
    errors = []
    metadata = policy.get("packageMetadata")
    if not isinstance(metadata, dict):
        return ["module-packages.json must include packageMetadata records"]

    package_ids = set()
    for section in PACKAGE_SECTIONS:
        package_ids.update(policy[section])

    missing_metadata = package_ids - set(metadata)
    if missing_metadata:
        errors.append(f"packageMetadata missing packages: {sorted(missing_metadata)}")

    stale_metadata = set(metadata) - package_ids
    if stale_metadata:
        errors.append(f"packageMetadata contains packages not in policy sections: {sorted(stale_metadata)}")

    for package, record in sorted(metadata.items()):
        if not isinstance(record, dict):
            errors.append(f"packageMetadata.{package} must be an object")
            continue
        for field in METADATA_FIELDS:
            value = record.get(field)
            if field == "allowedOwners":
                if not isinstance(value, list) or not value or any(not isinstance(item, str) or not item for item in value):
                    errors.append(f"packageMetadata.{package}.{field} must be a non-empty string list")
                else:
                    unknown_owners = sorted(set(value) - set(KNOWN_OWNERS))
                    if unknown_owners:
                        errors.append(f"packageMetadata.{package}.{field} contains unknown owners: {unknown_owners}")
                continue
            if not isinstance(value, str) or not value.strip():
                errors.append(f"packageMetadata.{package}.{field} must be a non-empty string")

    return errors


def load_items(path, item_name):
    tree = ET.parse(path)
    root = tree.getroot()
    return {
        item.attrib["Include"]
        for item in root.findall(f".//{item_name}")
        if "Include" in item.attrib
    }


def load_package_versions(path):
    tree = ET.parse(path)
    root = tree.getroot()
    return {
        item.attrib["Include"]: item.attrib["Version"]
        for item in root.findall(".//PackageVersion")
        if "Include" in item.attrib and "Version" in item.attrib
    }


def load_allowed_package_constants(path):
    text = path.read_text(encoding="utf-8")
    return {
        name: package_id
        for name, package_id in re.findall(
            r"public\s+const\s+string\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\"([^\"]+)\"",
            text)
    }


def load_allowlist_references(path, constants):
    text = path.read_text(encoding="utf-8")
    references = re.findall(r"AllowedPackageIds\.([A-Za-z_][A-Za-z0-9_]*)", text)
    packages = set()
    missing = []
    for reference in references:
        package = constants.get(reference)
        if package is None:
            missing.append(reference)
            continue
        packages.add(package)
    return packages, missing


def validate(policy_file, props_file, directory_packages_file, allowed_package_ids_file,
             module_package_allowlist_file, module_build_package_allowlist_file):
    policy = load_json_policy(policy_file)
    errors = validate_policy_metadata(policy)

    expected_module_direct = set(policy["runtimeDirect"]) | set(policy["buildDirect"])
    actual_module_direct = load_items(props_file, "OctarynAllowedModuleDirectPackageReference")
    if actual_module_direct != expected_module_direct:
        errors.append(
            "module direct PackageReference allowlist differs from module-packages.json: "
            f"expected {sorted(expected_module_direct)}, actual {sorted(actual_module_direct)}")

    expected_host_direct = set(policy["hostDirect"])
    actual_host_direct = load_items(props_file, "OctarynAllowedHostDirectPackageReference")
    if actual_host_direct != expected_host_direct:
        errors.append(
            "host direct PackageReference allowlist differs from host policy: "
            f"expected {sorted(expected_host_direct)}, actual {sorted(actual_host_direct)}")

    expected_host_only = set(policy["hostOnly"])
    actual_host_only = load_items(props_file, "OctarynHostOnlyPackageId")
    if actual_host_only != expected_host_only:
        errors.append(
            "host-only package list differs from host direct policy: "
            f"expected {sorted(expected_host_only)}, actual {sorted(actual_host_only)}")

    expected_versions = {}
    for section in ("runtimeDirect", "runtimeTransitive", "buildDirect", "buildTransitive", "hostDirect", "hostOnly"):
        expected_versions.update(policy[section])

    actual_versions = load_package_versions(directory_packages_file)
    for package, expected_version in sorted(expected_versions.items()):
        actual_version = actual_versions.get(package)
        if actual_version != expected_version:
            errors.append(
                f"Directory.Packages.props pin for {package} differs from package policy: "
                f"expected {expected_version}, actual {actual_version}")
    extra_versions = set(actual_versions) - set(expected_versions)
    if extra_versions:
        errors.append(
            "Directory.Packages.props contains stale package pins not present in module package policy: "
            f"{sorted(extra_versions)}")

    constants = load_allowed_package_constants(allowed_package_ids_file)
    expected_module_constants = expected_module_direct
    actual_module_constants = set(constants.values())
    if actual_module_constants != expected_module_constants:
        errors.append(
            "AllowedPackageIds.cs differs from module package policy: "
            f"expected {sorted(expected_module_constants)}, actual {sorted(actual_module_constants)}")

    actual_runtime_allowlist, runtime_missing = load_allowlist_references(module_package_allowlist_file, constants)
    if runtime_missing:
        errors.append(
            "ModulePackageAllowlist.cs references missing AllowedPackageIds constants: "
            f"{sorted(runtime_missing)}")
    if actual_runtime_allowlist != set(policy["runtimeDirect"]):
        errors.append(
            "ModulePackageAllowlist.cs differs from runtimeDirect policy: "
            f"expected {sorted(policy['runtimeDirect'])}, actual {sorted(actual_runtime_allowlist)}")

    actual_build_allowlist, build_missing = load_allowlist_references(module_build_package_allowlist_file, constants)
    if build_missing:
        errors.append(
            "ModuleBuildPackageAllowlist.cs references missing AllowedPackageIds constants: "
            f"{sorted(build_missing)}")
    if actual_build_allowlist != set(policy["buildDirect"]):
        errors.append(
            "ModuleBuildPackageAllowlist.cs differs from buildDirect policy: "
            f"expected {sorted(policy['buildDirect'])}, actual {sorted(actual_build_allowlist)}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy-file", default=str(DEFAULT_POLICY))
    parser.add_argument("--props-file", default=str(DEFAULT_PROPS))
    parser.add_argument("--directory-packages-file", default=str(DEFAULT_DIRECTORY_PACKAGES))
    parser.add_argument("--allowed-package-ids-file", default=str(DEFAULT_ALLOWED_PACKAGE_IDS))
    parser.add_argument("--module-package-allowlist-file", default=str(DEFAULT_MODULE_PACKAGE_ALLOWLIST))
    parser.add_argument("--module-build-package-allowlist-file", default=str(DEFAULT_MODULE_BUILD_PACKAGE_ALLOWLIST))
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.policy_file),
        pathlib.Path(args.props_file),
        pathlib.Path(args.directory_packages_file),
        pathlib.Path(args.allowed_package_ids_file),
        pathlib.Path(args.module_package_allowlist_file),
        pathlib.Path(args.module_build_package_allowlist_file))
    if errors:
        for error in errors:
            print(f"package policy sync: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
