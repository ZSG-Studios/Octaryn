"""Freeze exact native Linux client link inputs and matching static OpenAL source."""

import argparse
import gzip
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tarfile

from package_linux import digest, elf, regular, scan, verify_archive, write


def ninja_words(text):
    """Decode Ninja's path escapes without interpreting shell syntax."""
    words, word = [], ""
    index = 0
    while index < len(text):
        char = text[index]
        if char == "$" and index + 1 < len(text) and text[index + 1] in " $:":
            index += 1
            word += text[index]
        elif char.isspace():
            if word:
                words.append(word)
                word = ""
        else:
            word += char
        index += 1
    if word:
        words.append(word)
    return words


def link_graph(build):
    text = (build / "build.ninja").read_text().replace("$\n", "")
    lines = text.splitlines()
    indices = [i for i, line in enumerate(lines) if re.match(
        r"build .*client/native/bin/Octaryn\.Client: CXX_EXECUTABLE_LINKER_", line)]
    if len(indices) != 1:
        raise ValueError("Expected exactly one current native client executable link edge")
    index = indices[0]
    inputs = re.split(r": CXX_EXECUTABLE_LINKER_\S+ ", lines[index], maxsplit=1)[1]
    objects = ninja_words(inputs.split(" |", 1)[0])
    variables = {}
    for line in lines[index + 1:]:
        if not line.startswith("  "):
            break
        if " = " in line:
            key, value = line.strip().split(" = ", 1)
            variables[key] = value.replace("$$", "$")
    if not objects or "LINK_LIBRARIES" not in variables:
        raise ValueError("Client link edge lacks objects or libraries")
    if variables.get("PRE_LINK", ":") != ":" or variables.get("POST_BUILD", ":") != ":":
        raise ValueError("Custom link commands need explicit relink support")
    return objects, variables


def portable_flags(tokens):
    result, index = [], 0
    while index < len(tokens):
        token = tokens[index]
        if token == "-Xlinker" and index + 1 < len(tokens):
            argument = tokens[index + 1]
            if argument in ("-rpath", "-rpath-link"):
                index += 4 if index + 2 < len(tokens) and tokens[index + 2] == "-Xlinker" else 3
                continue
            if argument.startswith("--dependency-file="):
                index += 2
                continue
        if token.startswith("-Wl,--dependency-file="):
            index += 1
            continue
        result.append(token)
        index += 1
    return result


def source_copy(source, destination):
    """Copy source, preserving regular files; Git metadata is not corresponding source."""
    regular(source, directory=True)
    for current, dirs, names in os.walk(source, followlinks=False):
        dirs[:] = [n for n in dirs if n not in (".git", "__pycache__")]
        relative = Path(current).relative_to(source)
        target = destination / relative
        target.mkdir(parents=True, exist_ok=True)
        for name in dirs:
            regular(Path(current) / name, directory=True)
        for name in names:
            incoming = Path(current) / name
            regular(incoming)
            before = digest(incoming)
            shutil.copy2(incoming, target / name)
            if digest(incoming) != before or digest(target / name) != before:
                raise ValueError(f"Source changed during copy: {incoming}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo-root", "notices", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--preset", default="release-linux")
    parser.add_argument("--architecture", choices=("x64", "arm64"), default="x64")
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--name", required=True)
    args = parser.parse_args(argv)
    if os.name != "posix":
        raise ValueError("Run on the native Linux build host")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.name):
        raise ValueError("Invalid package name")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", args.preset):
        raise ValueError("Invalid preset directory name")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", args.source_commit):
        raise ValueError("Expected full source commit")
    repo, output, notices = args.repo_root.resolve(), args.output.resolve(), args.notices.resolve()
    build = repo / "build" / args.preset / "cmake"
    objects, variables = link_graph(build)
    elf(repo / "build" / args.preset / "client/native/bin/Octaryn.Client",
        62 if args.architecture == "x64" else 183)
    notice_before = scan(notices)
    inventory = json.loads((notices / "THIRD_PARTY/inventory.json").read_text())
    if inventory.get("missing_required") != [] or inventory.get("platform") != f"linux-{args.architecture}":
        raise ValueError("Need complete matching Linux notices")
    source = repo / "build/dependencies/src/openalsoft"
    if any(output.is_relative_to(path) for path in (notices, source, build)):
        raise ValueError("Output must be outside notices, source and link-input trees")
    stage, archive = output / args.name, output / f"{args.name}.tar.gz"
    checksum = output / f"{args.name}.tar.gz.sha256"
    if any(p.exists() or p.is_symlink() for p in (stage, archive, checksum)):
        raise ValueError("Refusing to overwrite relink outputs")
    output.mkdir(parents=True, exist_ok=True)
    stage.mkdir(mode=0o755)
    records, copied, response, system = [], {}, [], []

    def collect(value):
        original = Path(value)
        original = original if original.is_absolute() else build / original
        if not original.is_file():
            raise ValueError(f"Missing exact link input: {original}")
        resolved = original.resolve()
        # Configured SDK/host-pack shared libraries may legitimately be external.
        dynamic = ".so" in original.name
        if not resolved.is_relative_to(repo) and not dynamic:
            raise ValueError(f"Unexpected non-workspace static/object input: {original}")
        regular(resolved)
        if resolved not in copied:
            folder = "libraries" if dynamic or original.suffix == ".a" else "objects"
            filename = original.name if dynamic else f"{len(copied):04d}-{original.name}"
            relative = f"{folder}/{filename}"
            target = stage / relative
            target.parent.mkdir(exist_ok=True)
            before = digest(resolved)
            if target.exists() and digest(target) != before:
                raise ValueError(f"Shared-library basename collision: {filename}")
            if not target.exists():
                shutil.copy2(resolved, target)
            if digest(resolved) != before or digest(target) != before:
                raise ValueError(f"Link input changed: {original}")
            records.append(dict(file=relative, sha256=before, source=str(original),
                                resolved_source=str(resolved), dynamic=dynamic))
            copied[resolved] = relative
            if dynamic:
                prefix = original.name.split(".so", 1)[0] + ".so"
                for alias in original.parent.glob(prefix + "*"):
                    if alias.is_file() and alias.resolve() == resolved:
                        alias_target = stage / "libraries" / alias.name
                        if alias_target.exists() and digest(alias_target) != before:
                            raise ValueError(f"Shared-library alias collision: {alias.name}")
                        if not alias_target.exists():
                            shutil.copy2(resolved, alias_target)
        return copied[resolved]

    tokens = portable_flags(shlex.split(variables.get("FLAGS", "") + " " + variables.get("LINK_FLAGS", "")))
    tokens += objects + shlex.split(variables["LINK_LIBRARIES"])
    skip = False
    for token in tokens:
        if skip:
            skip = False
            continue
        if token in ("-rpath", "-rpath-link"):
            skip = True
            continue
        if token.startswith(("-Wl,-rpath,", "-Wl,-rpath=", "-Wl,-rpath-link,", "-Wl,-rpath-link=")):
            continue
        if token.startswith("-L"):
            # Every project/shared input is copied explicitly; do not retain build-host search paths.
            if token == "-L":
                skip = True
            system.append(token)
            continue
        if token.startswith("-"):
            if str(repo) in token or str(build) in token:
                raise ValueError(f"Untranslated build path in linker flag: {token}")
            response.append(token)
            if token.startswith("-l") or token == "-pthread":
                system.append(token)
        else:
            path = Path(token)
            if path.is_absolute() and not path.resolve().is_relative_to(repo) and ".so" in path.name and "dotnet" not in path.parts:
                # System shared libraries are supplied by the matching native toolchain.
                name = path.name
                flag = "-l" + name[3:-3] if name.startswith("lib") and name.endswith(".so") else "-l:" + name
                response.append(flag)
                system.append(flag + " (configured " + token + ")")
            else:
                response.append(collect(token))
    openal = [item for item in records if item["file"].endswith("-libopenal.a")]
    if len(openal) != 1:
        raise ValueError("Expected one exact static OpenAL library")
    response += ["-Wl,-rpath,$ORIGIN:$ORIGIN/libraries", "-Wl,-rpath-link,libraries", "-o", "Octaryn.Client"]
    write(stage / "client.rsp", "\n".join(shlex.quote(word) for word in response) + "\n")
    write(stage / "Relink.sh", '#!/bin/sh\nset -eu\ncd -- "$(dirname -- "$0")"\nexec clang++ @client.rsp\n', True)
    source_copy(source, stage / "openal-soft")
    source_before = scan(stage / "openal-soft")
    source_copy(notices, stage / "notices")
    shutil.copy2(repo / "LICENSE", stage / "LICENSE")
    cache = (build / "CMakeCache.txt").read_text()
    options = []
    for key, value in re.findall(r"^(ALSOFT_[A-Za-z0-9_]+):(?:BOOL|STRING)=(.*)$", cache, re.M):
        if "/" not in value and "\\" not in value:
            options.append(f"-D{key}={value}")
    options += ["-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++",
                "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", "-DLIBTYPE=STATIC"]
    command = shlex.join(["cmake", "-S", "openal-soft", "-B", "openal-build", "-G", "Ninja", *options])
    write(stage / "Rebuild-OpenAL.sh", '#!/bin/sh\nset -eu\ncd -- "$(dirname -- "$0")"\n'
          + command + '\ncmake --build openal-build --target OpenAL\n'
          + 'cp -- openal-build/libopenal.a ' + shlex.quote(openal[0]["file"]) + '\n./Relink.sh\n', True)
    version = subprocess.run(["clang++", "--version"], capture_output=True, text=True, check=True).stdout
    write(stage / "README.md", f'''# Native Linux client relink materials

Source commit: `{args.source_commit}`. Architecture: {args.architecture}.
The client statically links OpenAL Soft 1.25.1. This archive contains the exact
client objects and non-system link inputs, matching OpenAL source and copied
notices. Personal modification and reverse engineering to debug modifications
are permitted; dependency licenses remain with their respective authors.

Use native Clang/C++ standard library and development packages compatible with
the build host. Run `./Relink.sh` to produce `Octaryn.Client`. Use the complete
companion game package for assets, shaders, server and runtime libraries. Replace
only its client executable after preserving your original. RPATH uses $ORIGIN;
no build-host RPATH is retained. The supplied dynamic libraries are also found
under libraries/ when linking/checking the companion here.

Edit openal-soft/, then run `./Rebuild-OpenAL.sh` to build and replace the static
library and relink the supplied objects. The script records portable ALSOFT
options from the exact configured build. Required system link flags/search paths
were: `{shlex.join(system)}`. Install the corresponding system development
libraries; absolute search paths are not retained. These materials do not bundle
system glibc or the C++ runtime, and are not a portable all-distribution SDK.

Compiler reported at collection:
```
{version.strip()}
```

Packaging does not execute the linker or game. Actual replacement-library relink
and executable/runtime qualification must be recorded separately by the release
owner; this README is not a claim those checks passed.
''')
    payload = scan(stage)
    write(stage / "manifest.json", json.dumps(dict(schema=1, source_commit=args.source_commit,
          platform=f"linux-{args.architecture}", link_inputs=records, system_libraries=system,
          openal_options=options, files=payload, relink_qualification="not performed by packaging"), indent=2) + "\n")
    expected = scan(stage)
    with archive.open("xb") as raw, gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=0) as zipped:
        with tarfile.open(fileobj=zipped, mode="w", format=tarfile.PAX_FORMAT) as tar:
            for name, entry in expected.items():
                info = tarfile.TarInfo(f"{args.name}/{name}")
                info.size, info.mode = entry["size"], entry["mode"]
                with (stage / name).open("rb") as stream:
                    tar.addfile(info, stream)
    verify_archive(archive, args.name, expected)
    for item in records:
        if digest(Path(item["resolved_source"])) != item["sha256"]:
            raise ValueError("Link inputs changed during packaging")
    for name, item in source_before.items():
        if digest(source / name) != item["sha256"]:
            raise ValueError("OpenAL source changed during packaging")
    if scan(notices) != notice_before:
        raise ValueError("Notices changed during packaging")
    value = digest(archive)
    write(checksum, f"{value}  {archive.name}\n")
    print(json.dumps(dict(archive=str(archive), sha256=value, link_inputs=len(records), files=len(expected))))


if __name__ == "__main__":
    main()
