"""Exercise production SDK cache storage and Slang dependency keys without a GPU."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def process_cases(executable, fixture, environment):
    workers = []
    records = []
    try:
        for base in (4096, 8192):
            workers.append(subprocess.Popen([str(executable), '--cache-process', str(fixture / 'processes'), str(base)],
                                            cwd=ROOT, env=environment, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.PIPE))
        for worker in workers:
            timed_out = False
            try:
                stdout, stderr = worker.communicate(timeout=30)
            except subprocess.TimeoutExpired:
                timed_out = True
                worker.kill()
                stdout, stderr = worker.communicate(timeout=5)
            records.append(dict(returncode=worker.returncode, timed_out=timed_out, stdout=stdout, stderr=stderr,
                                passed=worker.returncode == 0 and not timed_out and
                                'shader_cache_process_writer=passed' in stdout))
    except OSError as error:
        records.append(dict(passed=False, error=str(error)))
    finally:
        for worker in workers:
            if worker.poll() is None:
                worker.kill()
                worker.communicate(timeout=5)
    if len(records) == 2 and all(record['passed'] for record in records):
        try:
            result = subprocess.run([str(executable), '--cache-process', str(fixture / 'processes'), 'inspect'],
                                    cwd=ROOT, env=environment, text=True, capture_output=True, timeout=30)
            records.append(dict(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr,
                                passed=result.returncode == 0 and 'shader_cache_processes=passed' in result.stdout))
        except (OSError, subprocess.TimeoutExpired) as error:
            records.append(dict(passed=False, error=str(error)))
    return records


def main():
    cache = {}
    for line in (ROOT / 'build/release-windows/cmake/CMakeCache.txt').read_text(encoding='utf-8').splitlines():
        if '=' in line and not line.startswith(('#', '//')):
            key, value = line.split('=', 1)
            cache[key.split(':', 1)[0]] = value
    owner = ROOT / 'octaryn-client/Source/Rendering/RenderBackend'
    sources = [owner / 'ShaderCache.cpp', owner / 'ShaderCacheStorage.cpp',
               ROOT / 'tools/validation/shader_cache_test.cpp', ROOT / 'tools/validation/shader_cache_eviction_test.cpp']
    sdk = Path(cache['OCTARYN_SLANG_SDK_ROOT'])
    external = [Path(cache[name]) / 'include' for name in
                ('OCTARYN_SLANG_RHI_SOURCE_ROOT', 'OCTARYN_SLANG_RHI_BUILD_ROOT', 'OCTARYN_SLANG_SDK_ROOT', 'SDL3_SOURCE_DIR')]
    output = ROOT / 'build/release-windows/tools/shader-cache'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    libraries = [str(Path(cache['SDL3_BINARY_DIR']) / 'SDL3-static.lib'), str(sdk / 'lib/slang-compiler.lib')]
    libraries += [name + '.lib' for name in ('kernel32', 'user32', 'gdi32', 'winmm', 'imm32', 'ole32',
                  'oleaut32', 'version', 'uuid', 'advapi32', 'setupapi', 'shell32', 'dinput8')]
    subprocess.run(['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20', '/W4', '/WX',
                    '/I' + str(owner), '/external:W0', *['/external:I' + str(path) for path in external],
                    *map(str, sources), '/Fe:shader_cache_test.exe', '/link', *libraries],
                   cwd=output, check=True, timeout=120)
    environment = dict(os.environ)
    environment['PATH'] = str(sdk / 'bin') + os.pathsep + environment.get('PATH', '')
    fixture = Path(tempfile.mkdtemp(prefix='case-', dir=output)) / 'data'
    result = subprocess.run([str(output / 'shader_cache_test.exe'), str(fixture)], cwd=ROOT,
                            env=environment, text=True, capture_output=True, timeout=60)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    markers = {marker: marker in result.stdout for marker in
               ('shader_cache_test=passed', 'shader_cache_eviction=passed')}
    processes = process_cases(output / 'shader_cache_test.exe', fixture, environment) if result.returncode == 0 else []
    passed = result.returncode == 0 and all(markers.values()) and len(processes) == 3 and all(p['passed'] for p in processes)
    report = dict(status='passed' if passed else 'failed', production_cache=True,
                  real_slang_keys=True, gpu_runtime=False, returncode=result.returncode, fixture=str(fixture),
                  source_sha256={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in sources + [owner / 'ShaderCache.h', owner / 'ShaderCacheStorage.h']},
                  markers=markers, processes=processes, stdout=result.stdout, stderr=result.stderr)
    path = ROOT / 'logs/tools/shader-cache.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    result.check_returncode()
    if not passed:
        raise RuntimeError(f'Shader cache qualification failed; see {path}')


if __name__ == '__main__':
    main()
