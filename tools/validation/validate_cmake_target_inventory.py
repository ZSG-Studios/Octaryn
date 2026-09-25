#!/usr/bin/env python3
import argparse
import json
import pathlib
import re
import sys


from validate_cmake_target_inventory_policy import (
    ALLOWED_LOG_ROOTS,
    ALLOWED_PRESET_SUBROOTS,
    FORBIDDEN_ACTIVE_WORKSPACE_PATHS,
    FORBIDDEN_BUILD_FILE_NAMES,
    FORBIDDEN_BUILD_SUBROOT_NAMES,
    FORBIDDEN_CMAKE_PATHS,
    FORBIDDEN_TARGET_PATTERNS,
    REQUIRED_BUILD_COMMAND_SNIPPETS,
    REQUIRED_BUILD_PRESETS,
    REQUIRED_CMAKE_STRUCTURE,
    REQUIRED_CONFIGURE_PRESET_TOOLCHAINS,
    REQUIRED_CONFIGURE_PRESETS,
    REQUIRED_TARGETS,
    STATIC_ALLOWED_BUILD_ROOTS,
)


def load_targets(build_file):
    targets = set()
    target_pattern = re.compile(r"^build\s+([^:| ]+)")
    for line in build_file.read_text(encoding="utf-8").splitlines():
        match = target_pattern.match(line)
        if not match:
            continue
        target = match.group(1)
        if target.startswith("/") or target.startswith("CMakeFiles/"):
            continue
        targets.add(target)
    return targets


def preset_platform(preset_name):
    if preset_name.endswith("-arm64"):
        preset_name = preset_name[:-6]
    if preset_name.endswith("-windows"):
        return "windows"
    return "linux"


def validate_hostfxr_target_state(build_text, preset_name):
    errors = []
    if "Cannot validate hostfxr bridge exports without native hosting." in build_text:
        return errors  # The explicitly requested runtime target fails; configuration may cross-compile.
    for snippet in ("validate_hostfxr_bridge_exports.py", "validate_owner_launch_probe_logs.py"):
        if snippet not in build_text:
            errors.append(f"native hosting graph is missing {snippet}")
    return errors


def validate_presets(repo_root):
    presets_path = repo_root / "CMakePresets.json"
    if not presets_path.exists():
        return [f"{presets_path}: missing CMake presets"]

    presets = json.loads(presets_path.read_text(encoding="utf-8"))
    configure = {preset["name"] for preset in presets.get("configurePresets", [])}
    build = {preset["name"] for preset in presets.get("buildPresets", [])}
    errors = []

    missing_configure = sorted(set(REQUIRED_CONFIGURE_PRESETS) - configure)
    if missing_configure:
        errors.append(f"missing required configure presets: {missing_configure}")
    unexpected_configure = sorted(configure - set(REQUIRED_CONFIGURE_PRESETS))
    if unexpected_configure:
        errors.append(f"unexpected configure presets: {unexpected_configure}")

    missing_build = sorted(set(REQUIRED_BUILD_PRESETS) - build)
    if missing_build:
        errors.append(f"missing required build presets: {missing_build}")
    unexpected_build = sorted(build - set(REQUIRED_BUILD_PRESETS))
    if unexpected_build:
        errors.append(f"unexpected build presets: {unexpected_build}")

    for preset in presets.get("configurePresets", []):
        expected_binary_dir = "${sourceDir}/build/${presetName}/cmake"
        if preset.get("binaryDir") != expected_binary_dir:
            errors.append(
                f"configure preset {preset['name']} must use binaryDir {expected_binary_dir}, got {preset.get('binaryDir')}")
        expected_toolchain = REQUIRED_CONFIGURE_PRESET_TOOLCHAINS.get(preset["name"])
        if expected_toolchain and preset.get("toolchainFile") != expected_toolchain:
            errors.append(
                f"configure preset {preset['name']} must use toolchainFile {expected_toolchain}, got {preset.get('toolchainFile')}")

    for preset in presets.get("buildPresets", []):
        configure_preset = preset.get("configurePreset")
        if configure_preset and configure_preset not in configure:
            errors.append(f"build preset {preset['name']} references missing configure preset {configure_preset}")
        targets = preset.get("targets", [])
        if targets != ["octaryn_all"]:
            errors.append(
                f"build preset {preset['name']} must target only octaryn_all, got {targets}")

    return errors


def configure_preset_build_dirs(repo_root):
    presets_path = repo_root / "CMakePresets.json"
    if not presets_path.exists():
        return []

    presets = json.loads(presets_path.read_text(encoding="utf-8"))
    build_dirs = []
    for preset in presets.get("configurePresets", []):
        name = preset["name"]
        binary_dir = preset.get("binaryDir")
        if not binary_dir:
            continue

        resolved = binary_dir.replace("${sourceDir}", str(repo_root)).replace("${presetName}", name)
        build_dirs.append((name, pathlib.Path(resolved)))

    return build_dirs


def configured_graph_build_dirs(repo_root):
    build_dirs = list(configure_preset_build_dirs(repo_root))
    build_dirs.extend(
        (f"{preset_name}-arm64", build_dir.parent.parent / f"{preset_name}-arm64" / "cmake")
        for preset_name, build_dir in configure_preset_build_dirs(repo_root)
    )
    return build_dirs


def allowed_build_root_names(repo_root):
    preset_names = {
        preset_name
        for preset_name, _build_dir in configure_preset_build_dirs(repo_root)
    }
    names = set(STATIC_ALLOWED_BUILD_ROOTS)
    names.update(preset_names)
    names.update(f"{preset_name}-arm64" for preset_name in preset_names)
    return names


def validate_aggregate_dependencies(build_text):
    graph = {}
    for line in build_text.splitlines():
        match = re.match(r"build (octaryn_[^ :]+): phony (.*)", line)
        if match:
            graph[match.group(1)] = set(re.findall(r"\boctaryn_[A-Za-z0-9_]+\b", match.group(2)))
    reached, pending = set(), ["octaryn_validate_all"]
    while pending:
        target = pending.pop()
        if target in reached:
            continue
        reached.add(target)
        pending.extend(graph.get(target, ()))
    required = {name for name in REQUIRED_TARGETS if name.startswith("octaryn_validate_")}
    # The old app-launch names are aliases of the directly required RHI diagnostic.
    required.discard("octaryn_validate_client_app_launch_probe")
    return [f"validation aggregates do not reach {name}" for name in sorted(required - reached)]


def validate_critical_command_snippets(build_text):
    errors = []
    for snippet in REQUIRED_BUILD_COMMAND_SNIPPETS:
        if snippet not in build_text:
            errors.append(f"configured build graph is missing critical command snippet {snippet}")
    return errors


def validate_generated_layout(repo_root):
    errors = []
    configured_preset_names = {
        preset_name
        for preset_name, _build_dir in configure_preset_build_dirs(repo_root)
    }
    allowed_build_roots = allowed_build_root_names(repo_root)
    for root_name, allowed in (("build", allowed_build_roots), ("logs", ALLOWED_LOG_ROOTS)):
        root = repo_root / root_name
        if not root.exists():
            continue

        forbidden = [
            path.name
            for path in root.iterdir()
            if path.is_dir() and path.name not in allowed
        ]
        if forbidden:
            errors.append(f"forbidden generated {root_name}/ roots: {sorted(forbidden)}")

        root_files = sorted(path.name for path in root.iterdir() if path.is_file())
        if root_files:
            errors.append(f"forbidden generated {root_name}/ files: {root_files}")

    build_root = repo_root / "build"
    if build_root.exists():
        forbidden_build_subroots = []
        stale_preset_subroots = []
        for preset_name in configured_preset_names:
            preset_root = build_root / preset_name
            if not preset_root.exists():
                continue

            for path in preset_root.rglob("*"):
                if path.is_dir() and path.name in FORBIDDEN_BUILD_SUBROOT_NAMES:
                    forbidden_build_subroots.append(path.relative_to(repo_root).as_posix())
                if path.is_file() and path.name in FORBIDDEN_BUILD_FILE_NAMES:
                    forbidden_build_subroots.append(path.relative_to(repo_root).as_posix())

            for path in preset_root.iterdir():
                if path.is_dir() and path.name not in ALLOWED_PRESET_SUBROOTS:
                    stale_preset_subroots.append(path.relative_to(repo_root).as_posix())

            cmake_generated_root = preset_root / "cmake" / "generated"
            if cmake_generated_root.exists():
                stale_preset_subroots.append(cmake_generated_root.relative_to(repo_root).as_posix())

        if forbidden_build_subroots:
            errors.append(
                "dependency build/stamp roots must live under build/<preset>/deps: "
                f"{sorted(forbidden_build_subroots)}")
        if stale_preset_subroots:
            errors.append(
                "preset build roots must contain only approved owner/tool/dependency roots: "
                f"{sorted(stale_preset_subroots)}")

    return errors


def validate_active_workspace_paths(repo_root):
    forbidden_paths = [
        path
        for path in FORBIDDEN_ACTIVE_WORKSPACE_PATHS
        if (repo_root / path).exists()
    ]
    if forbidden_paths:
        return [f"forbidden active workspace structure paths: {forbidden_paths}"]
    return []


def derive_repo_root(build_dir):
    resolved = build_dir.resolve()
    for parent in [resolved, *resolved.parents]:
        if (parent / "CMakeLists.txt").exists() and (parent / "cmake").exists():
            return parent
    return resolved


def validate_single_build_dir(build_dir, repo_root):
    build_file = build_dir / "build.ninja"
    if not build_file.exists():
        return [f"{build_file}: missing configured Ninja graph"]

    build_text = build_file.read_text(encoding="utf-8")
    targets = load_targets(build_file)
    errors = []

    missing = REQUIRED_TARGETS - targets
    if missing:
        errors.append(f"missing active CMake targets: {sorted(missing)}")

    forbidden = [
        target
        for target in sorted(targets)
        if any(pattern in target for pattern in FORBIDDEN_TARGET_PATTERNS)
    ]
    if forbidden:
        errors.append(f"forbidden active CMake targets: {forbidden}")

    missing_structure = [path for path in REQUIRED_CMAKE_STRUCTURE if not (repo_root / path).exists()]
    if missing_structure:
        errors.append(f"missing required CMake owner/platform/dependency files: {missing_structure}")

    forbidden_paths = [path for path in FORBIDDEN_CMAKE_PATHS if exact_path_exists(repo_root, path)]
    if forbidden_paths:
        errors.append(f"forbidden active CMake structure paths: {forbidden_paths}")

    preset_name = build_dir.parent.name if build_dir.name == "cmake" else build_dir.name
    errors.extend(validate_hostfxr_target_state(build_text, preset_name))
    errors.extend(validate_presets(repo_root))
    errors.extend(validate_aggregate_dependencies(build_text))
    errors.extend(validate_critical_command_snippets(build_text))

    return errors


def exact_path_exists(root, relative):
    for part in pathlib.PurePosixPath(relative).parts:
        if not root.is_dir():
            return False
        match = next((entry for entry in root.iterdir() if entry.name == part), None)
        if match is None:
            return False
        root = match
    return True


def validate(build_dir, repo_root):
    errors = validate_single_build_dir(build_dir, repo_root)
    errors.extend(validate_active_workspace_paths(repo_root))
    errors.extend(validate_generated_layout(repo_root))
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--repo-root")
    args = parser.parse_args()

    build_dir = pathlib.Path(args.build_dir)
    repo_root = pathlib.Path(args.repo_root).resolve() if args.repo_root else derive_repo_root(build_dir)
    errors = validate(build_dir, repo_root)
    if errors:
        for error in errors:
            print(f"cmake target inventory: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
