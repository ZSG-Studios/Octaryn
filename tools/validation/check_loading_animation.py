"""Validate supplied authored loading markup/styles with the production RmlUi clock."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
import vsenv


def confined(root, relative):
    if not isinstance(relative, str) or ':' in relative or '\\' in relative:
        raise ValueError('Probe resource path must be package relative')
    parts = relative.split('/')
    if any(part in ('', '.', '..') for part in parts):
        raise ValueError('Probe resource path escapes package')
    path = (root / relative).resolve(strict=True)
    if not path.is_relative_to(root) or not path.is_file():
        raise ValueError('Probe resource outside package')
    return path


def source_profile(args, output):
    """Resolve generic indexed meshes; source expectations are supplied externally."""
    screen_path = args.screen.resolve(strict=True)
    root = screen_path.parent
    screen = json.loads(screen_path.read_text(encoding='utf-8'))
    resource_index = json.loads((root / 'resources.json').read_text(encoding='utf-8'))
    indexed = {row['id']: row for row in resource_index['resources']}
    used = []
    def resource(resource_id, kind):
        row = indexed[resource_id]
        if row['kind'] != kind:
            raise ValueError('Probe resource kind differs')
        path = confined(root, row['path'])
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != row['sha256']:
            raise ValueError('Probe indexed resource hash differs')
        used.append({'path': str(path), 'sha256': row['sha256']})
        return path, data
    meshes = []
    for binding in screen['meshes']:
        path, data = resource(binding['resource'], 'ui.mesh2d')
        mesh = json.loads(data)
        texture, _ = resource(mesh['texture'], 'image')
        meshes.append({'element': binding['element'], 'texture': str(texture).replace('\\', '/'),
                       'vertices': mesh['vertices'], 'indices': mesh['indices'], 'wrap': mesh.get('wrap', False)})
    source = json.loads(args.mesh_expectations.read_text(encoding='utf-8'))
    expected = []
    for part in source['parts']:
        if part['hidden']:
            continue
        animation = part['animation']
        expected.append({'element': part['element'], 'seconds': animation['seconds'] if animation else 0,
                         'endpoint_degrees': animation['keyframes'][-1]['degrees'] if animation else 0,
                         'vertices': part['mesh']['vertices'], 'indices': part['mesh']['indices']})
    if len(meshes) != 4 or {m['element'] for m in meshes} != {m['element'] for m in expected}:
        raise ValueError('Probe source visible mesh bindings differ')
    markup = args.document.read_text(encoding='utf-8')
    markup = markup.replace('</head>', '<style>' + args.style.read_text(encoding='utf-8') + '</style></head>', 1)
    declaration = {'version': 2, 'model': 'declared_document', 'screen_id': 'fixture.loading',
                   'logical_width': screen['logical_width'], 'logical_height': screen['logical_height'],
                   'modal': True, 'markup': markup, 'meshes': meshes,
                   'fields': [{'id': 'status', 'element': screen['fields'][0]['element']}]}
    profile = {'declaration': json.dumps(declaration, separators=(',', ':')), 'meshes': expected,
               'slide_seconds': args.slide_seconds if args.slide_seconds is not None else 10,
               'fade_seconds': args.fade_seconds if args.fade_seconds is not None else 20 / 9,
               'initial_delay_seconds': 10, 'fade_starts_at_event': True}
    path = output / 'source-profile.json'
    path.write_text(json.dumps(profile, indent=2) + '\n', encoding='utf-8')
    used += [{'path': str(screen_path), 'sha256': hashlib.sha256(screen_path.read_bytes()).hexdigest()},
             {'path': str(args.mesh_expectations.resolve()), 'sha256': hashlib.sha256(args.mesh_expectations.read_bytes()).hexdigest()},
             {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}]
    return path, used


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--document', type=Path, required=True)
    parser.add_argument('--style', type=Path, required=True)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/windows-x64/tools/loading-animation')
    parser.add_argument('--screen', type=Path)
    parser.add_argument('--mesh-expectations', type=Path)
    parser.add_argument('--slide-seconds', type=float)
    parser.add_argument('--fade-seconds', type=float)
    args = parser.parse_args()
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if bool(args.screen) != bool(args.mesh_expectations):
        raise ValueError('Source mesh probe needs screen and independent expectations together')
    profile, source_inputs = source_profile(args, output) if args.screen else (None, [])
    vs = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs, 'x64')
    vsenv.prepend_tool_dirs(ROOT, vs, 'x64')
    executable = output / 'LoadingAnimationProbe.exe'
    command = [vsenv.resolve_tool('clang-cl'), '/nologo', '/std:c++latest', '/EHsc', '/MD', '/O2',
               '/DRMLUI_STATIC_LIB', '/I' + str(ROOT / 'build/dependencies/src/rmlui/Include'),
               '/I' + str(ROOT / 'build/dependencies/src/glaze/include'),
               str(ROOT / 'tools/Source/LoadingAnimationProbe/main.cpp'),
               str(ROOT / 'octaryn-client/Source/Ui/DeclaredScreen/DeclaredDocumentUi.cpp'),
               '/Fe' + str(executable), '/Fo' + str(output) + '/', '/link',
               str(ROOT / 'build/release-windows/deps/lib/rmlui.lib'),
               str(ROOT / 'build/release-windows/deps/build/freetype/freetype.lib'), 'user32.lib']
    started = time.perf_counter()
    receipt = {'kind': 'authored-loading-animation', 'productionRml': True, 'gpu': False,
               'osInput': False, 'assetAdmission': False, 'command': command, 'status': 'failed',
               'inputs': [{'path': str(path.resolve()), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                          for path in (args.document, args.style)] + source_inputs,
               'sourceMeshGeometryAndControllers': bool(profile), 'bitmapFontAdmission': False}
    with (output / 'checks.log').open('w', encoding='utf-8') as log:
        compiled = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=120)
        receipt['compileExitCode'] = compiled.returncode
        if compiled.returncode == 0:
            invocation = [str(executable), str(args.document.resolve()), str(args.style.resolve())]
            if profile:
                invocation.append(str(profile))
            checked = subprocess.run(invocation,
                                     cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=30)
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
