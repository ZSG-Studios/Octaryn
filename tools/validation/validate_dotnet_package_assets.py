#!/usr/bin/env python3
import argparse
import json
import pathlib
import sys


DEFAULT_POLICY = pathlib.Path(__file__).resolve().parents[1] / "package-policy" / "module-packages.json"
PACKAGE_SECTIONS = ("runtimeDirect", "runtimeTransitive", "buildDirect", "buildTransitive", "hostDirect", "hostOnly")
METADATA_FIELDS = ("allowedOwners", "purpose", "versionPolicy", "runtimeScope", "validationRule", "enforcement")
KNOWN_OWNERS = ("basegame", "client", "game", "game-module", "mod", "server")


def package_id(asset_key):
    return asset_key.rsplit("/", 1)[0]


def package_version(asset_key):
    return asset_key.rsplit("/", 1)[1]


def has_real_runtime_asset(target_entry):
    runtime_assets = target_entry.get("runtime", {})
    return any(not path.endswith("/_._") for path in runtime_assets)


def has_real_compile_asset(target_entry):
    compile_assets = target_entry.get("compile", {})
    return any(not path.endswith("/_._") for path in compile_assets)


def dependency_ids(target_entry):
    return set(target_entry.get("dependencies", {}).keys())


def closure_from(start_ids, target_packages):
    pending = list(start_ids)
    visited = set()
    while pending:
        current = pending.pop()
        if current in visited:
            continue
        visited.add(current)
        entry = target_packages.get(current)
        if entry is None:
            continue
        pending.extend(dependency_ids(entry) - visited)
    return visited


def load_target_packages(target):
    packages = {}
    for key, entry in target.items():
        if entry.get("type") != "package":
            continue
        packages[package_id(key)] = {
            "version": package_version(key),
            "entry": entry,
        }
    return packages


def check_versions(errors, packages, policy, label):
    for package, expected in policy.items():
        actual = packages.get(package, {}).get("version")
        if actual is not None and actual != expected:
            errors.append(f"{label} package {package} resolved {actual}, expected {expected}")


def load_policy(policy_file):
    with open(policy_file, "r", encoding="utf-8") as file:
        policy = json.load(file)

    validate_policy_metadata(policy)

    return {
        "runtime_direct": policy["runtimeDirect"],
        "runtime_transitive": policy["runtimeTransitive"],
        "build_direct": policy["buildDirect"],
        "build_transitive": policy["buildTransitive"],
        "host_direct": policy["hostDirect"],
        "host_only": policy["hostOnly"],
        "metadata": policy["packageMetadata"],
    }


def validate_policy_metadata(policy):
    metadata = policy.get("packageMetadata")
    if not isinstance(metadata, dict):
        raise ValueError("module package policy must include packageMetadata records")

    package_ids = set()
    for section in PACKAGE_SECTIONS:
        package_ids.update(policy[section])

    missing = package_ids - set(metadata)
    stale = set(metadata) - package_ids
    if missing:
        raise ValueError(f"packageMetadata missing packages: {sorted(missing)}")
    if stale:
        raise ValueError(f"packageMetadata contains packages not in policy sections: {sorted(stale)}")

    for package, record in metadata.items():
        if not isinstance(record, dict):
            raise ValueError(f"packageMetadata.{package} must be an object")
        for field in METADATA_FIELDS:
            value = record.get(field)
            if field == "allowedOwners":
                if not isinstance(value, list) or not value or any(not isinstance(item, str) or not item for item in value):
                    raise ValueError(f"packageMetadata.{package}.{field} must be a non-empty string list")
                unknown_owners = sorted(set(value) - set(KNOWN_OWNERS))
                if unknown_owners:
                    raise ValueError(f"packageMetadata.{package}.{field} contains unknown owners: {unknown_owners}")
                continue
            if not isinstance(value, str) or not value.strip():
                raise ValueError(f"packageMetadata.{package}.{field} must be a non-empty string")


def validate(assets_file, policy_file, owner):
    if owner in {"shared", "old-architecture"}:
        return []

    with open(assets_file, "r", encoding="utf-8") as file:
        assets = json.load(file)

    try:
        policy = load_policy(policy_file)
    except ValueError as error:
        return [str(error)]
    if owner in {"client", "server"}:
        return validate_host_assets(assets, policy, owner)

    direct_allowed = set(policy["runtime_direct"]) | set(policy["build_direct"])
    runtime_transitive_allowed = set(policy["runtime_transitive"])
    build_transitive_allowed = set(policy["build_transitive"])

    direct_project_packages = set()
    for dependencies in assets.get("projectFileDependencyGroups", {}).values():
        for dependency in dependencies:
            direct_project_packages.add(dependency.split(" ", 1)[0])

    errors = []
    for target_name, target in assets.get("targets", {}).items():
        packages = load_target_packages(target)
        package_entries = {name: data["entry"] for name, data in packages.items()}
        runtime_graph = closure_from(policy["runtime_direct"], package_entries)
        build_graph = closure_from(policy["build_direct"], package_entries)

        target_errors = []
        check_versions(target_errors, packages, policy["runtime_direct"], "direct runtime")
        check_versions(target_errors, packages, policy["runtime_transitive"], "transitive runtime")
        check_versions(target_errors, packages, policy["build_direct"], "direct build")
        check_versions(target_errors, packages, policy["build_transitive"], "transitive build")

        for package, data in packages.items():
            entry = data["entry"]
            is_direct = package in direct_project_packages
            is_build_graph = package in build_graph
            is_runtime_asset = has_real_runtime_asset(entry)
            is_compile_asset = has_real_compile_asset(entry)

            if not is_owner_allowed(policy, package, owner):
                target_errors.append(f"package {package} is not allowed for owner {owner}")
                continue

            if is_direct and package not in direct_allowed:
                target_errors.append(f"unapproved direct package {package}")
                continue

            if is_direct and package in policy["runtime_direct"]:
                if not is_runtime_asset:
                    target_errors.append(f"direct runtime package {package} has no real runtime asset")
                continue

            if is_direct and package in policy["build_direct"]:
                if is_runtime_asset or is_compile_asset:
                    target_errors.append(f"direct build package {package} exposes runtime or compile assets")
                continue

            if package in runtime_transitive_allowed:
                if package not in runtime_graph:
                    target_errors.append(f"runtime transitive package {package} is not reachable from approved runtime packages")
                    continue
                if not is_runtime_asset:
                    target_errors.append(f"runtime transitive package {package} has no real runtime asset")
                continue

            if package in build_transitive_allowed:
                if not is_build_graph:
                    target_errors.append(f"build transitive package {package} is not reachable from approved build packages")
                continue

            if package in policy["build_direct"]:
                continue

            target_errors.append(f"unclassified package {package}/{data['version']}")

        errors.extend(f"{target_name}: {error}" for error in target_errors)

    return errors


def validate_host_assets(assets, policy, owner):
    direct_allowed = set(policy["host_direct"])
    allowed_packages = (
        set(policy["host_direct"]) |
        set(policy["runtime_transitive"]) |
        set(policy["build_transitive"]))

    direct_project_packages = set()
    for dependencies in assets.get("projectFileDependencyGroups", {}).values():
        for dependency in dependencies:
            direct_project_packages.add(dependency.split(" ", 1)[0])

    errors = []
    expected_versions = {}
    for key in ("host_direct", "runtime_transitive", "build_transitive"):
        expected_versions.update(policy[key])

    for target_name, target in assets.get("targets", {}).items():
        packages = load_target_packages(target)
        target_errors = []
        check_versions(target_errors, packages, expected_versions, f"{owner} allowed")

        for package, data in packages.items():
            if not is_owner_allowed(policy, package, owner):
                target_errors.append(f"package {package} is not allowed for owner {owner}")
                continue

            if package not in allowed_packages:
                target_errors.append(f"unclassified host package {package}/{data['version']}")
                continue

            if package in direct_project_packages and package not in direct_allowed:
                target_errors.append(f"unapproved host direct package {package}")

        errors.extend(f"{target_name}: {error}" for error in target_errors)

    return errors


def is_owner_allowed(policy, package, owner):
    metadata = policy["metadata"].get(package)
    if metadata is None:
        return False

    return owner in set(metadata["allowedOwners"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets-file", required=True)
    parser.add_argument("--owner", default="module")
    parser.add_argument("--policy-file", default=str(DEFAULT_POLICY))
    args = parser.parse_args()

    errors = validate(args.assets_file, args.policy_file, args.owner)
    if errors:
        for error in errors:
            print(f"dotnet package policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
