#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from fsr2_vendor import vendor_root, verified_files

# Preserved older entry files have no stage annotation.
LEGACY_ENTRIES = (
    ("Voxel/VoxelFacePull.vert.slang", "vertex", "main"),
    ("Voxel/VoxelDepth.frag.slang", "fragment", "main"),
    ("Voxel/VoxelPbr.frag.slang", "fragment", "main"),
    ("Voxel/VoxelVisibility.frag.slang", "fragment", "main"),
)
STAGES = {"vertex", "fragment", "compute", "geometry", "hull", "domain", "mesh", "amplification",
          "raygeneration", "intersection", "anyhit", "closesthit", "miss", "callable"}
COMMENTS = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"')
ANNOTATION = re.compile(r'\[\s*shader\s*\(\s*"([^"]+)"\s*\)\s*\]')
SIGNATURE = re.compile(r'\s*(?:\[[^\]]*\]\s*)*[\w:<>,\s]+?\b([A-Za-z_]\w*)\s*\(')


def annotated_entries(text):
    text = COMMENTS.sub(lambda m: "\n" * m[0].count("\n") if m[0].startswith(("//", "/*")) else m[0], text)
    entries = []
    for annotation in ANNOTATION.finditer(text):
        stage = annotation[1]
        if stage not in STAGES:
            raise ValueError(f"unsupported shader stage: {stage}")
        signature = SIGNATURE.match(text, annotation.end())
        if not signature:
            raise ValueError(f"shader annotation has no function declaration: {annotation[0]}")
        entries.append((stage, signature[1]))
    return entries


def parser_self_test():
    source = '\n'.join((
        '// [shader("compute")] void ignored() {}',
        '/* [shader("fragment")] float4 ignored2() {} */',
        '[shader("vertex")] Out vertex_main(uint id) {}',
        '[shader("fragment")] float4 fragment_main(In x):SV_Target {}',
        '[shader("fragment")] float4 forward_main(In x):SV_Target {}',
        '[shader("compute")] [numthreads(8,8,1)] void update(uint3 id) {}',
    ))
    assert annotated_entries(source) == [("vertex", "vertex_main"), ("fragment", "fragment_main"),
                                        ("fragment", "forward_main"), ("compute", "update")]
    assert annotated_entries('float4 main():SV_Target { return 1; }') == []
    for invalid in ('[shader("typo")] void main() {}', '[shader("compute")] float4;'):
        try:
            annotated_entries(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError(f"invalid entry annotation accepted: {invalid}")
    print("slang_entry_parser=passed positive_entries=4 comment_decoys=2 legacy_unannotated=1 negative_annotations=2")


FORBIDDEN_PLACEHOLDER_SNIPPETS = (
    "float4(0.0, 0.0, 0.0, 1.0)",
    "float4(1.0, 1.0, 1.0, 1.0)",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--slangc", default=None)
    parser.add_argument("--target", choices=("spirv", "dxil", "metal"), default="spirv")
    parser.add_argument("--profile", default=None)
    parser.add_argument("--dxc", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        parser_self_test()
        return 0
    if not args.source_root or not args.output_dir:
        parser.error("--source-root and --output-dir are required for shader compilation")
    args.source_root = args.source_root.resolve()

    slangc = args.slangc or shutil.which("slangc")
    if not slangc:
        print("slangc was not found in PATH; Slang shader validation cannot run", file=sys.stderr)
        return 1

    errors: list[str] = []
    results = []
    profile = args.profile or {"spirv": "spirv_1_3", "dxil": "sm_6_8", "metal": "sm_6_8"}[args.target]
    environment = os.environ.copy()
    if args.dxc:
        environment["PATH"] = str(args.dxc.resolve().parent) + os.pathsep + environment.get("PATH", "")
    args.output_dir.mkdir(parents=True, exist_ok=True)

    active_shader_files = sorted(path for path in args.source_root.rglob("*") if path.is_file())
    non_slang_sources = [path.relative_to(args.source_root).as_posix() for path in active_shader_files if path.suffix != ".slang"]
    if non_slang_sources:
        errors.append(f"active shader tree must be Slang-only; non-Slang files found: {non_slang_sources}")

    shader_files = {path.resolve() for path in active_shader_files if path.suffix == ".slang"}
    fsr_vendor = vendor_root(args.source_root.parents[1])
    fsr_files = set()
    if (args.source_root / "Fsr2/DepthClip.slang").is_file():
        try:
            fsr_files = verified_files(fsr_vendor)
        except (OSError, ValueError, KeyError) as error:
            errors.append(f"FSR2 vendor validation failed: {error}")
    includes: dict[Path, set[Path]] = {}
    for source in shader_files:
        dependencies: set[Path] = set()
        for name in re.findall(r'^\s*#include\s+"([^"]+)"', source.read_text(), re.MULTILINE):
            if source.parent == args.source_root / "Fsr2" and name in fsr_files and name.endswith('.hlsl'):
                continue
            target = (source.parent / name).resolve()
            if target not in shader_files:
                target = (args.source_root / name).resolve()
            if target not in shader_files:
                errors.append(f"shader include is missing or outside the source tree: {source}: {name}")
            else:
                dependencies.add(target)
        includes[source] = dependencies
    included = set().union(*includes.values()) if includes else set()
    roots = shader_files - included
    reachable: set[Path] = set()
    pending = list(roots)
    while pending:
        source = pending.pop()
        if source in reachable:
            continue
        reachable.add(source)
        pending.extend(includes[source])
    if reachable != shader_files:
        errors.append(f"shader include cycle has no compilation root: {sorted(shader_files - reachable)}")
    # Include fragments use declarations from their parent. Compile each complete
    # translation unit, and prove every source is reached by those compilations.
    for source in sorted(roots):
        relative = source.relative_to(args.source_root.resolve()).as_posix()
        output = args.output_dir / (relative.replace("/", "_") + ".slang-module")
        result = subprocess.run(
            [slangc, str(source), "-I", str(args.source_root), "-I", str(source.parent), "-I", str(fsr_vendor),
             "-emit-ir", "-o", str(output)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=environment, timeout=120)
        output.with_suffix(".module.log").write_text(result.stdout, encoding="utf-8")
        if result.returncode != 0 or not output.exists() or output.stat().st_size == 0:
            errors.append(f"Slang module compilation failed for {source}:\n{result.stdout}")

    entry_points = set(LEGACY_ENTRIES)
    for source in roots:
        if source.parent == args.source_root / "Fsr2":
            entry_points.add((source.relative_to(args.source_root).as_posix(), "compute", "CS"))
    for root in roots:
        dependencies, pending = set(), [root]
        while pending:
            source = pending.pop()
            if source in dependencies:
                continue
            dependencies.add(source)
            pending.extend(includes[source])
        for source in dependencies:
            try:
                for stage, entry in annotated_entries(source.read_text()):
                    entry_points.add((root.relative_to(args.source_root).as_posix(), stage, entry))
            except ValueError as error:
                errors.append(f"{source}: {error}")
    for relative, stage, entry in sorted(entry_points):
        source = args.source_root / relative
        if not source.exists():
            errors.append(f"missing Slang entry source: {source}")
            continue
        source_text = source.read_text()
        for snippet in FORBIDDEN_PLACEHOLDER_SNIPPETS:
            if snippet in source_text:
                errors.append(f"{source}: forbidden placeholder shader output {snippet!r}")
        extension = {"spirv": "spv", "dxil": "dxil", "metal": "metal"}[args.target]
        output = args.output_dir / (relative.replace("/", "_") + f".{entry}.{stage}.{extension}")
        command = [
            slangc,
            str(source),
            "-I",
            str(args.source_root),
            "-I",
            str(source.parent),
            "-I",
            str(fsr_vendor),
            "-entry",
            entry,
            "-stage",
            stage,
            "-target",
            args.target,
            "-profile", profile,
            "-o",
            str(output),
        ]
        if args.target == "spirv":
            command += ["-emit-spirv-directly"]
        elif args.target == "metal":
            command += ["-capability", "metallib_3_1"]
        for hdr in (0, 1) if relative.startswith("Fsr2/") else (None,):
            case_output = output if hdr is None else output.with_name(output.stem + f".hdr{hdr}" + output.suffix)
            case_command = command.copy()
            case_command[case_command.index("-o") + 1] = str(case_output)
            if hdr is not None:
                case_command += [f"-DFFX_FSR2_OPTION_HDR_COLOR_INPUT={hdr}"]
            result = subprocess.run(case_command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    env=environment, timeout=120)
            case_output.with_suffix(case_output.suffix + ".log").write_text(result.stdout, encoding="utf-8")
            produced = case_output.exists() and case_output.stat().st_size > 0
            results.append({"source": relative, "stage": stage, "entry": entry, "hdr": hdr,
                            "exit": result.returncode, "produced": produced, "command": case_command})
            if result.returncode != 0:
                errors.append(f"slangc failed for {source} ({stage}, hdr={hdr}) with exit {result.returncode}:\n{result.stdout}")
            elif not produced:
                errors.append(f"slangc produced no {args.target} output for {source}")

    report = {"target": args.target, "profile": profile,
              "scope": "Metal source emission only; no Apple compilation/runtime" if args.target == "metal" else "shader compilation only",
              "compiler_sha256": hashlib.sha256(Path(slangc).read_bytes()).hexdigest(),
              "sources_sha256": {p.relative_to(args.source_root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in sorted(shader_files)},
              "fsr_vendor_manifest_sha256": hashlib.sha256((fsr_vendor / "manifest.json").read_bytes()).hexdigest() if fsr_files else None,
              "modules": len(roots), "entry_points": len(entry_points), "cases": results, "errors": errors}
    (args.output_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"client_slang_shader_validation=passed sources={len(shader_files)} modules={len(roots)} entry_points={len(entry_points)} cases={len(results)} target={args.target} profile={profile} slangc={slangc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
