"""Compile optional screen intersections plus every changed raster output contract."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    output = ROOT / 'build/release-windows/tools/reflection-screen'
    output.mkdir(parents=True, exist_ok=True)
    entries = []
    for suffix in ('', 'Coherent'):
        for entry in ('classify_main', 'screen_main', 'intersect_main', 'shade_main', 'recovery_main'):
            entries.append((f'Hdr/MapReflectionQueueScreen{suffix}.slang', entry))
    entries += [('Hdr/MapReflectionCoverage.slang', entry) for entry in
                ('coverage_vertex', 'coverage_item_vertex', 'coverage_fragment')]
    entries += [('Map/WorldMap.slang', 'fragment_main'), ('Map/WorldMapRT.slang', 'forward_main'),
                ('Map/MapMeshlets.slang', 'mesh_main'), ('Map/MapMeshlets.slang', 'fragment_main'),
                ('Items/WorldItem.slang', 'item_vertex'), ('Items/WorldItem.slang', 'item_fragment')]
    compiler = ROOT / 'build/dependencies/slang-2026.17.1/bin/slangc.exe'
    results = []
    for target, profile in [('dxil', 'sm_6_6'), ('spirv', 'spirv_1_5')]:
        for path, entry in entries:
            shader = ROOT / 'octaryn-client/Shaders' / path
            command = [str(compiler), str(shader), '-entry', entry, '-target', target,
                       '-profile', 'sm_6_8' if target == 'dxil' and entry in ('coverage_item_vertex', 'item_vertex') else profile, '-o', str(output / f'{shader.stem}-{entry}.{target}')]
            if target == 'dxil':
                command += ['-dxc-path', str(ROOT / 'build/dependencies/slang-rhi-windows-x64-Release/_deps/dxc-src/bin/x64')]
            process = subprocess.run(command, text=True, capture_output=True)
            results.append(dict(path=path, entry=entry, target=target, exit_code=process.returncode,
                                diagnostic=process.stdout + process.stderr))
    (output / 'compilation.json').write_text(json.dumps(results, indent=2))
    failed = [result for result in results if result['exit_code']]
    if failed:
        raise RuntimeError(json.dumps(failed, indent=2))
    print(f'reflection_screen_compiled={len(results)} targets=DXIL,SPIR-V gpu_runtime=not_run')


if __name__ == '__main__':
    main()
