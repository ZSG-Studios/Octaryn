#!/usr/bin/env python3
import argparse
import subprocess
import pathlib
import sys
import xml.etree.ElementTree as ET


DEFAULT_OWNER_RULES = {
    "shared": set(),
    "basegame": {"shared"},
    "game": {"shared"},
    "game-module": {"shared"},
    "mod": {"shared"},
    "client": {"shared"},
    "server": {"shared"},
    "tools": {"shared", "basegame", "game", "game-module", "mod", "client", "server"},
}


def normalize(path):
    return path.resolve()


def load_owner(project_file):
    result = subprocess.run(
        [
            "dotnet",
            "msbuild",
            str(project_file),
            "-getProperty:OctarynBuildOwner",
            "-nologo",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        message = result.stderr.strip() or result.stdout.strip() or f"exit code {result.returncode}"
        raise RuntimeError(f"{project_file}: failed to evaluate OctarynBuildOwner: {message}")

    owner = result.stdout.strip()
    if not owner:
        raise RuntimeError(f"{project_file}: OctarynBuildOwner evaluated to an empty value")
    return owner


def project_references(project_file):
    tree = ET.parse(project_file)
    root = tree.getroot()
    references = []
    for item in root.findall(".//ProjectReference"):
        include = item.attrib.get("Include")
        if include:
            references.append(normalize(project_file.parent / include.replace("\\", "/")))
    return references


def direct_references(project_file):
    tree = ET.parse(project_file)
    root = tree.getroot()
    references = []
    for item in root.findall(".//Reference"):
        include = item.attrib.get("Include")
        if include:
            references.append(include)
    return references


def expected_owner(repo_root, project_file):
    name = project_file.stem
    project_path = project_file.as_posix()
    try:
        relative_path = project_file.relative_to(repo_root).as_posix()
    except ValueError:
        relative_path = project_path

    if name == "Octaryn.Shared":
        return "shared"
    if name == "Octaryn.Basegame":
        return "basegame"
    if name == "Octaryn.Client":
        return "client"
    if name == "Octaryn.Server":
        return "server"
    if "/tools/validation/" in project_path or relative_path.startswith("tools/validation/"):
        return "tools"
    if "/references/old-architecture/" in project_path or relative_path.startswith("references/old-architecture/"):
        return "old-architecture"
    if "/octaryn-games/" in project_path or relative_path.startswith("octaryn-games/"):
        return "game"
    if "/octaryn-modules/" in project_path or relative_path.startswith("octaryn-modules/"):
        return "game-module"
    if "/octaryn-mods/" in project_path or relative_path.startswith("octaryn-mods/"):
        return "mod"
    return "unknown"


def validate(repo_root, project_file):
    project_file = normalize(project_file)
    owners = {}
    owner = project_owner(owners, project_file)
    expected = expected_owner(repo_root, project_file)
    allowed = DEFAULT_OWNER_RULES.get(owner)
    errors = []

    if expected != "unknown" and owner != expected:
        errors.append(f"{project_file}: evaluated owner {owner} does not match expected owner {expected}")

    if owner == "unknown":
        errors.append(f"{project_file}: unknown Octaryn project owner")
        return errors
    if owner == "old-architecture":
        return errors
    if allowed is None:
        errors.append(f"{project_file}: no project reference boundary rule for owner {owner}")
        return errors

    if owner in {"basegame", "game", "game-module", "mod"} and direct_references(project_file):
        errors.append(f"{project_file}: module owners cannot use direct binary Reference items: {direct_references(project_file)}")

    for reference in project_references(project_file):
        referenced_owner = project_owner(owners, reference)
        expected_reference_owner = expected_owner(repo_root, reference)
        if expected_reference_owner != "unknown" and referenced_owner != expected_reference_owner:
            errors.append(
                f"{reference}: evaluated owner {referenced_owner} does not match expected owner {expected_reference_owner}")
        if referenced_owner not in allowed:
            errors.append(
                f"{project_file}: owner {owner} cannot reference {reference} "
                f"owned by {referenced_owner}; allowed owners are {sorted(allowed)}")

    return errors


def project_owner(owners, project_file):
    project_file = normalize(project_file)
    if project_file not in owners:
        owners[project_file] = load_owner(project_file)
    return owners[project_file]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--project-file", required=True)
    args = parser.parse_args()

    try:
        errors = validate(pathlib.Path(args.repo_root).resolve(), pathlib.Path(args.project_file).resolve())
    except RuntimeError as error:
        print(f"project reference boundary policy: {error}", file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print(f"project reference boundary policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
