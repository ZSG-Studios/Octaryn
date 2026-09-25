#!/usr/bin/env python3
"""Compile actual ray paths and reject uniform indexing of divergent resources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SKY_PROBE = '''#include "WorldSkyVisibility.slang"
StructuredBuffer<float4> origins;
RWStructuredBuffer<float> visibility;
[shader("compute")] [numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    float4 origin=origins[id.x];
    visibility[id.x]=world_sky_transmission(origin.xyz,normalize(float3(.8,.1,.2)),64);
}
'''
ENTRIES = ('RayTracing/Debug.slang', 'RayTracing/Shadow.slang', 'RayTracing/DescriptorSkyProbe.slang')


def inspect_hlsl(source):
    accesses = re.findall(r'(?:Resource|Sampler)DescriptorHeap\[([^\n;]+)\]', source)
    bad = [access for access in accesses if 'NonUniformResourceIndex(' not in access]
    return dict(accesses=len(accesses), uniform_accesses=bad,
                passed=len(accesses) >= 2 and not bad)


def inspect_dxil(source):
    calls = re.findall(r'@dx\.op\.createHandleFromHeap\(i32 218, i32 [^,]+, i1 (true|false), i1 (true|false)\)', source)
    return dict(accesses=len(calls), sampler_accesses=sum(sampler == 'true' for sampler, _ in calls),
                uniform_accesses=sum(nonuniform != 'true' for _, nonuniform in calls),
                passed=len(calls) >= 2 and all(nonuniform == 'true' for _, nonuniform in calls))


def inspect_spirv(source):
    decorated = set(re.findall(r'OpDecorate (%\S+) NonUniform\b', source))
    accesses = re.findall(r'(%\S+) = OpAccessChain %\S+ (%__slang_resource_heap\S*) (%\S+)', source)
    bad = [dict(result=result, heap=heap, index=index) for result, heap, index in accesses
           if result not in decorated or index not in decorated]
    return dict(accesses=len(accesses), nonuniform_decorations=len(decorated), uniform_accesses=bad,
                storage_buffer_capability='OpCapability StorageBufferArrayNonUniformIndexing' in source,
                passed=len(accesses) >= 2 and not bad)


def compile_entry(compiler, source_root, entry, target, profile, output, environment):
    source = source_root / entry
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [str(compiler), str(source), '-entry', 'main', '-stage', 'compute',
               '-target', target, '-profile', profile, '-I', str(source_root), '-o', str(output)]
    process = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=90)
    output.with_suffix(output.suffix + '.log').write_text(process.stdout + process.stderr)
    if process.returncode:
        raise RuntimeError(f'{entry} {target} compile failed: {process.stderr}')
    text = output.read_text()
    check = {'hlsl': inspect_hlsl, 'dxil-asm': inspect_dxil, 'spirv-asm': inspect_spirv}[target](text)
    return dict(entry=entry, target=target, profile=profile, output=str(output),
                sha256=hashlib.sha256(output.read_bytes()).hexdigest(), **check)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--slangc', type=Path, default=ROOT / 'build/dependencies/slang-2026.17.1/bin/slangc.exe')
    parser.add_argument('--dxc-dir', type=Path,
                        default=ROOT / 'build/dependencies/slang-rhi-windows-x64-Release/_deps/dxc-src/bin/x64')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'logs/tools/ray-descriptor-indices')
    parser.add_argument('--spirv-profile', default=None,
                        help='Defaults to the actual Vulkan runtime targetProfile')
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    device = (ROOT / 'octaryn-client/Source/Rendering/RenderBackend/WorldRendererDevice.cpp').read_text()
    runtime_profile = re.search(r'targetProfile="(spirv_\d_\d)"', device)[1]
    spirv_profile = args.spirv_profile or runtime_profile
    environment = os.environ.copy()
    environment['PATH'] = str(args.dxc_dir.resolve()) + os.pathsep + environment.get('PATH', '')
    run = Path(tempfile.mkdtemp(prefix='emission-', dir=args.output_dir.resolve()))
    fixed = run / 'fixed'
    shutil.copytree(ROOT / 'octaryn-client/Shaders', fixed)
    (fixed / 'RayTracing/DescriptorSkyProbe.slang').write_text(SKY_PROBE)
    broken = run / 'negative-control'
    shutil.copytree(fixed, broken)
    removed = 0
    for relative in ('RayTracing/WorldRayQuery.slang', 'RayTracing/WorldSkyVisibility.slang'):
        path = broken / relative
        source, count = re.subn(r'nonuniform\((record\.(?:faces|fluids))\)', r'\1', path.read_text())
        path.write_text(source)
        removed += count
    errors = []
    if removed != 4:
        errors.append(f'Expected to remove four actual voxel resource markers, removed {removed}')
    results = []
    for label, source_root in (('fixed', fixed), ('negative-control', broken)):
        for entry in ENTRIES:
            for target, profile in (('hlsl', 'sm_6_6'), ('dxil-asm', 'sm_6_6'), ('spirv-asm', spirv_profile)):
                output = run / 'emitted' / label / (Path(entry).stem + '.' + target + '.txt')
                result = compile_entry(args.slangc.resolve(), source_root, entry, target, profile, output, environment)
                result['variant'] = label
                results.append(result)
                expected = label == 'fixed'
                if result['passed'] != expected:
                    errors.append(f'{label} {entry} {target}: expected marker gate={expected}, got {result["passed"]}')
    report = dict(runtime_spirv_profile=runtime_profile, inspected_spirv_profile=spirv_profile,
                  source_snapshot=str(fixed), negative_markers_removed=removed,
                  checks=len(results), results=results, errors=errors, passed=not errors)
    path = run / 'result.json'
    path.write_text(json.dumps(report, indent=2))
    print(json.dumps(dict(passed=not errors, checks=len(results), errors=errors, report=str(path)), indent=2))
    return int(bool(errors))


if __name__ == '__main__':
    raise SystemExit(main())
