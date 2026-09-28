"""Compile/run fake-dispatch tests of the actual GPA owner without loading a GPU DLL."""
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(root / 'tools/build'))
    sys.path.insert(0, str(root / 'tools/build/support'))
    from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs
    from acquire_gpu_perf_api import pin, api_version
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(root, vs, 'x64')
    spec = pin(root)
    sdk = root / 'build/dependencies/tools/gpu-perf-api' / spec['TAG'] / 'package' / spec['SOURCE_SUBDIR']
    output = root / 'build/release-windows/tools/gpu-counter-probe'
    output.mkdir(parents=True, exist_ok=True)
    logs = root / 'logs/build/gpu-counter-probe'
    logs.mkdir(parents=True, exist_ok=True)
    ninja = (root / 'build/release-windows/cmake/build.ninja').read_text()
    anchor = 'build CMakeFiles/octaryn_client_render_backend.dir/octaryn-client/Source/MapWorld/MapRendererIndirect.cpp.obj:'
    start = ninja.index(anchor)
    block = ninja[start:ninja.index('\n\n', start)]
    args = []
    for key in ('DEFINES', 'INCLUDES'):
        args += shlex.split(re.search(r'  ' + key + r' = (.*)', block)[1])
    args = [arg for arg in args if 'OCTARYN_GPA_' not in arg]
    args += ['/I' + str(sdk / 'include'), '/DOCTARYN_GPA_DLL="unused-not-loaded.dll"', '/UNDEBUG']
    for name, value in zip(('MAJOR', 'MINOR', 'BUILD', 'UPDATE'), api_version(spec['TAG'])):
        args.append('/DOCTARYN_GPA_' + name + '=' + str(value))
    owner = root / 'octaryn-client/Source/Rendering/RenderBackend'
    executable = output / 'gpu-counter-probe.exe'
    command = ['clang-cl', '/nologo', '/std:c++20', '/EHsc', '/MD', *args,
               str(root / 'tools/validation/GpuCounterProfileProbe.cpp'),
               str(owner / 'GpuCounterSdk.cpp'), str(owner / 'FrameWatchdog.cpp'),
               '/Fe:' + str(executable)]
    with (logs / 'compile.log').open('wb') as stream:
        subprocess.run(command, cwd=output, stdout=stream, stderr=subprocess.STDOUT, check=True)
    with (logs / 'runtime.log').open('wb') as stream:
        subprocess.run([str(executable), str(logs)], stdout=stream, stderr=subprocess.STDOUT, check=True)
    with (logs / 'unbalanced.log').open('wb') as stream:
        fatal = subprocess.run([str(executable), str(logs), 'unbalanced'], stdout=stream, stderr=subprocess.STDOUT)
    if fatal.returncode != 1 or 'world_gpu_shutdown_failed stage=gpu_counter_scope' not in (logs / 'unbalanced.log').read_text():
        raise AssertionError('Unbalanced native recording scope did not fail closed')
    complete = [json.loads(line) for line in (logs / 'complete.jsonl').read_text().splitlines()]
    assert [row['value'] for row in complete if row['event'] == 'result'] == [42, 12.5]
    assert sum(row['event'] == 'complete' for row in complete) == 1
    inventory = [json.loads(line) for line in (logs / 'inventory.jsonl').read_text().splitlines()]
    assert sum(row['event'] == 'counter' for row in inventory) == 4
    assert {row['reason'] for row in inventory if row['event'] == 'excluded'} == {
        'would_require_multipass', 'not_available_discrete'}
    failure = [json.loads(line) for line in (logs / 'timeout.jsonl').read_text().splitlines()]
    assert any(row.get('operation') == 'nonblocking_readback_deadline' for row in failure)
    print('gpu_counter_probe=passed real_owner_fake_api=1 gpu_device_created=0 dll_loaded=0')


if __name__ == '__main__':
    main()
