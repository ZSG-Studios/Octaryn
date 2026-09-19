"""CPU-only execution of the exact native atlas binder with explicit RHI doubles."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def binder_source():
    source = ROOT / "octaryn-client/Source/Rendering/Atlas/WorldAtlas.cpp"
    text = source.read_text(encoding="utf-8")
    signature = "bool bind_world_atlas(WorldAtlas* atlas,rhi::IShaderObject* root)"
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def run():
    output = ROOT / "build/release-windows/tools/world-atlas-binding"
    output.mkdir(parents=True, exist_ok=True)
    log = ROOT / "logs/tools/world-atlas-binding.json"
    log.parent.mkdir(parents=True, exist_ok=True)
    report = dict(status="failed", gpu_runtime=False, production_binder_body=True,
                  doubles=["WorldAtlas", "rhi::ShaderCursor", "rhi::IShaderObject", "rhi::Binding"],
                  limitations="Does not execute atlas loading, real RHI reflection/layout, or GPU submission.",
                  cases={})
    try:
        body = binder_source()
        report["binder_sha256"] = hashlib.sha256(body.encode()).hexdigest()
        catalog_path = ROOT / "octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json"
        catalog_text = catalog_path.read_text(encoding="utf-8")
        catalog = json.loads(catalog_text)
        blocks = catalog["blocks"]
        if catalog["schema"] != "octaryn.basegame.blocks.v1" or \
                blocks[0]["id"] != "octaryn.basegame.block.air":
            raise ValueError("Expected the production block catalog with air at index zero")
        report["catalog_count_including_air"] = len(blocks)
        report["catalog_sha256"] = hashlib.sha256(catalog_text.encode()).hexdigest()
        (output / "CatalogCount.h").write_text(
            f"constexpr std::uint32_t catalog_count = {len(blocks)}u;\n", encoding="utf-8")
        # Remove only the count-upload fix, retaining all preexisting resource bindings.
        old_body, changes = re.subn(
            r'  const auto count=static_cast<std::uint32_t>\(atlas->material_flags.size\(\)\);\s*'
            r'auto trace_count=cursor\["voxelTraceMaterialCount"\];\s*'
            r'if\(trace_count.isValid\(\) && SLANG_FAILED\(trace_count.setData\(&count,sizeof\(count\)\)\)\)return false;\s*',
            "", body)
        if changes != 1:
            raise ValueError("Count-upload negative control must remove exactly one known fix")
        vs = find_vs_root()
        import_vs_environment(vs, "x64")
        prepend_tool_dirs(ROOT, vs, "x64")
        for name, variant in (("production", body), ("old_missing_count", old_body)):
            directory = output / name
            directory.mkdir(exist_ok=True)
            (directory / "WorldAtlasBinder.h").write_text(variant + "\n", encoding="utf-8")
            build = subprocess.run(
                ["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/I" + str(directory),
                 "/I" + str(output), str(ROOT / "tools/validation/world_atlas_binding_test.cpp"),
                 "/Fe:world_atlas_binding_test.exe"], cwd=directory, text=True, capture_output=True)
            (log.parent / f"world-atlas-binding-{name}-compile.log").write_text(
                build.stdout + build.stderr, encoding="utf-8")
            build.check_returncode()
            result = subprocess.run([str(directory / "world_atlas_binding_test.exe")],
                                    cwd=ROOT, text=True, capture_output=True)
            report["cases"][name] = dict(exit_code=result.returncode, stdout=result.stdout,
                                         stderr=result.stderr)
            print(f"{name}: exit={result.returncode}\n{result.stdout}{result.stderr}", end="")
            if name == "production":
                result.check_returncode()
            elif result.returncode != 1 or "FAIL: catalog count including air was not uploaded" not in result.stderr:
                raise AssertionError("Original missing-count binder must fail the count-value assertion")
        report["status"] = "passed"
    finally:
        log.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"world_atlas_binding passed; report={log.relative_to(ROOT)}")


if __name__ == "__main__":
    run()
