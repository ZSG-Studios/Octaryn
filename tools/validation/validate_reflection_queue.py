"""Compile every queued reflection stage; does not claim GPU runtime correctness."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    output = ROOT / 'build/release-windows/tools/reflection-queue'
    output.mkdir(parents=True, exist_ok=True)
    compiler = ROOT / 'build/dependencies/slang-2026.17.1/bin/slangc.exe'
    entries = ('clear_main', 'classify_main', 'arguments_main', 'intersect_main', 'shade_main', 'recovery_main',
               'recovery_classify_main', 'recovery_refine_main')
    results = []
    variants = [(entry, False) for entry in entries]
    variants += [(entry, True) for entry in entries if entry not in ('clear_main', 'arguments_main')]
    for target, profile in [('spirv', 'spirv_1_5'), ('dxil', 'sm_6_6')]:
        for entry, coherent in variants:
            name = 'MapReflectionQueueArguments' if entry in ('clear_main', 'arguments_main') else 'MapReflectionQueue'
            if entry in ('recovery_classify_main', 'recovery_refine_main'):
                name = 'MapReflectionRecovery'
            if coherent:
                name += 'Coherent'
            shader = ROOT / f'octaryn-client/Shaders/Hdr/{name}.slang'
            command = [str(compiler), str(shader), '-entry', entry, '-target', target,
                       '-profile', profile, '-o', str(output / f'{entry}{"-coherent" if coherent else ""}.{target}')]
            if target == 'dxil':
                command += ['-dxc-path', str(ROOT / 'build/dependencies/slang-rhi-windows-x64-Release/_deps/dxc-src/bin/x64')]
            process = subprocess.run(command, text=True, capture_output=True)
            results.append(dict(entry=entry, coherent=coherent, target=target, exit_code=process.returncode,
                                diagnostics=process.stdout + process.stderr))
    (output / 'compilation.json').write_text(json.dumps(results, indent=2))
    failed = [result for result in results if result['exit_code']]
    if failed:
        raise RuntimeError(json.dumps(failed, indent=2))
    print(f'reflection_queue_shader_targets=DXIL,SPIR-V variants={len(variants)} compiled={len(results)} gpu_runtime=not_run')


if __name__ == '__main__':
    main()
