"""Exercise production declared UI/RmlUi interaction without OS input or a GPU."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
import vsenv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/windows-x64/tools/declared-interaction')
    args = parser.parse_args()
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    vs = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs, 'x64')
    vsenv.prepend_tool_dirs(ROOT, vs, 'x64')
    executable = output / 'DeclaredInteractionProbe.exe'
    command = [vsenv.resolve_tool('clang-cl'), '/nologo', '/std:c++latest', '/EHsc', '/MD', '/O2',
               '/DRMLUI_STATIC_LIB', '/I' + str(ROOT / 'build/dependencies/src/rmlui/Include'),
               '/I' + str(ROOT / 'build/dependencies/src/glaze/include'),
               str(ROOT / 'tools/Source/DeclaredInteractionProbe/main.cpp'),
               str(ROOT / 'octaryn-client/Source/Ui/DeclaredScreen/DeclaredDocumentUi.cpp'),
               '/Fe' + str(executable), '/Fo' + str(output) + '/', '/link',
               str(ROOT / 'build/release-windows/deps/lib/rmlui.lib'),
               str(ROOT / 'build/release-windows/deps/build/freetype/freetype.lib'), 'user32.lib']
    started = time.perf_counter()
    receipt = {'kind': 'production-declared-ui-authored-input', 'gpu': False, 'osInput': False,
               'productionRml': True, 'command': command, 'status': 'failed'}
    with (output / 'checks.log').open('w', encoding='utf-8') as log:
        compiled = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=120)
        receipt['compileExitCode'] = compiled.returncode
        if compiled.returncode == 0:
            checked = subprocess.run([str(executable)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=20)
            receipt['checksExitCode'] = checked.returncode
            if checked.returncode == 0:
                receipt['status'] = 'passed'
    receipt['seconds'] = time.perf_counter() - started
    (output / 'result.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print((output / 'checks.log').read_text(encoding='utf-8'))
    print(json.dumps({'status': receipt['status'], 'receipt': str(output / 'result.json')}))
    return 0 if receipt['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
