"""Package the exact client link inputs and OpenAL Soft source for relinking."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def ninja_words(text):
    # Ninja escapes spaces and drive colons with $, not shell backslashes.
    escaped = text.replace("$ ", "\0").replace("$:", ":").replace("$$", "$")
    return [word.replace("\0", " ").strip('"') for word in escaped.split()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    repo = args.repo_root.resolve()
    build = repo / "build/release-windows/cmake"
    lines = (build / "build.ninja").read_text(encoding="utf-8").splitlines()
    index = next(i for i, line in enumerate(lines)
                 if line.startswith("build ") and "/client/native/bin/Octaryn.Client.exe:" in line)
    inputs = lines[index].split("CXX_EXECUTABLE_LINKER__octaryn_client_app_Release ", 1)[1]
    objects = ninja_words(inputs.split(" |", 1)[0])
    block = []
    for line in lines[index + 1:]:
        if not line.strip():
            break
        block.append(line)
    variables = dict(line.strip().split(" = ", 1) for line in block if " = " in line)
    libraries = ninja_words(variables["LINK_LIBRARIES"])
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    name = "octaryn-slang-rhi-preview-relink-windows-x64-20260914"
    stage, archive = output / name, output / (name + ".zip")
    if stage.exists() or archive.exists():
        raise RuntimeError("Refusing to overwrite an existing relink package")
    stage.mkdir()
    records, response, copied = [], [], {}

    def collect(value, category):
        source = Path(value)
        if not source.is_absolute():
            source = build / source
        if not source.is_file():
            if category == "libraries" and len(Path(value).parts) == 1:
                response.append(value)  # Windows SDK / Visual C++ standard import library.
                return
            raise FileNotFoundError(source)
        source = source.resolve()
        if not source.is_relative_to(repo) or source.is_symlink():
            raise ValueError(f"Unexpected non-workspace link input: {source}")
        if source not in copied:
            relative = f"{category}/{len(copied):03d}-{source.name}"
            destination = stage / relative
            destination.parent.mkdir(exist_ok=True)
            before = digest(source)
            shutil.copy2(source, destination)
            if digest(source) != before or digest(destination) != before:
                raise RuntimeError(f"Link input changed while packaging: {source.name}")
            records.append({"file": relative, "sha256": before,
                            "source": source.relative_to(repo).as_posix()})
            copied[source] = relative
        response.append('"' + copied[source] + '"')

    for value in objects:
        collect(value, "objects")
    for value in libraries:
        collect(value, "libraries")
    openal = next(item["file"] for item in records if item["file"].endswith("-OpenAL32.lib"))
    source = repo / "build/dependencies/src/openalsoft"
    shutil.copytree(source, stage / "openal-soft",
                    ignore=shutil.ignore_patterns(".git", "__pycache__", "*.pyc"))
    # Include source notices next to the relink inputs as well as the game archive.
    notices = output / "notices-draft"
    if not (notices / "THIRD_PARTY_NOTICES.txt").is_file():
        raise RuntimeError("Collect release notices into output/notices-draft first")
    notice_inventory = json.loads((notices / "THIRD_PARTY/inventory.json").read_text(encoding="utf-8"))
    if notice_inventory.get("missing_required") != []:
        raise RuntimeError("Notice collection is incomplete")
    shutil.copytree(notices, stage, dirs_exist_ok=True)
    shutil.copy2(repo / "LICENSE", stage / "LICENSE")
    flags = variables["LINK_FLAGS"] + " /nologo /out:Octaryn.Client.exe /version:0.0"
    (stage / "client.rsp").write_text(flags + "\n" + "\n".join(response) + "\n", encoding="utf-8")
    (stage / "Relink.ps1").write_text(
        "$ErrorActionPreference = 'Stop'\nPush-Location $PSScriptRoot\ntry {\n"
        "    & lld-link.exe '@client.rsp'\n"
        "    if ($LASTEXITCODE -ne 0) { throw 'Relinking failed' }\n"
        "} finally { Pop-Location }\n", encoding="utf-8")
    (stage / "README.md").write_text(f"""# Octaryn client relink materials

For the native Windows x64 Slang RHI Preview of 2026-09-14.
Source commit: `{args.source_commit}`.

The client statically links OpenAL Soft 1.25.1. This archive includes the
corresponding OpenAL Soft source, its COPYING and other source notices, the exact
Octaryn client object files and non-system libraries, and a relocatable linker
response file. Personal modification and reverse engineering to debug such
modifications are permitted. Octaryn source is MIT licensed at the release tag;
the bundled dependencies retain their respective licenses.

## Relink the supplied objects

Use an x64 Visual Studio C++ developer PowerShell with Windows SDK and LLVM
(`clang-cl` and `lld-link`) installed. From this folder run `./Relink.ps1`.
The resulting `Octaryn.Client.exe` needs the complete companion game archive
(DLLs, shaders, assets, data, and server); replace only that archive's client EXE.
The Windows SDK/Visual C++ system libraries are supplied by those development
tools and are not duplicated here. `manifest.json` maps every supplied link input.

## Modify OpenAL Soft

Edit the supplied `openal-soft/` source, then configure from developer PowerShell:

```powershell
cmake -S openal-soft -B openal-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DLIBTYPE=STATIC -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF
cmake --build openal-build --target OpenAL
Copy-Item openal-build/OpenAL32.lib '{openal}'
./Relink.ps1
```

Keep x64, static library and dynamic CRT settings consistent. The release was
built with clang-cl/LLVM and Visual C++ 19.51. Source rebuilding requires CMake
and Ninja in addition to the linker prerequisites. Do not overwrite your only
copy of the original game executable when experimenting.
""", encoding="utf-8")
    inventory = {path.relative_to(stage).as_posix(): digest(path)
                 for path in sorted(stage.rglob("*")) if path.is_file()}
    (stage / "manifest.json").write_text(json.dumps({"source_commit": args.source_commit,
        "link_inputs": records, "files": inventory}, indent=2) + "\n", encoding="utf-8")
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zip_file:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                zip_file.write(path, (Path(name) / path.relative_to(stage)).as_posix())
    with zipfile.ZipFile(archive) as zip_file:
        if zip_file.testzip() is not None:
            raise RuntimeError("Relink ZIP failed CRC validation")
    checksum = digest(archive)
    archive.with_suffix(".zip.sha256").write_text(f"{checksum}  {archive.name}\n", encoding="ascii")
    print(json.dumps({"archive": str(archive), "sha256": checksum,
                      "link_inputs": len(records), "files": len(inventory)}))


if __name__ == "__main__":
    main()
