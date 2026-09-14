#!/usr/bin/env python3
"""Audit the configured client dependency closure, not dormant reference sources."""
import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from fsr2_vendor import vendor_root, verified_files

COMMENT = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', re.M)
SHADER_STRING = re.compile(r'"([^"\n]+\.(?:slang|glsl|hlsl|vert|frag|comp))"', re.I)
FORBIDDEN = (
    ('Slang GFX API', re.compile(r'\bgfx\s*::|\bgfxCreate\w*\s*\(|slang-gfx\.h')),
    ('raw Vulkan API', re.compile(r'\bvk[A-Z]\w*\s*\(|\bVk[A-Z]\w*\b|#\s*include\s*[<"]vulkan/')),
    ('SDL GPU renderer', re.compile(r'\bSDL_GPU\w*\b|\bSDL_\w*GPU\w*\s*\(')),
    ('non-Slang shader path', re.compile(r'"[^"\n]+\.(?:glsl|hlsl|vert|frag|comp)"', re.I)),
    ('raw OpenGL API', re.compile(r'\bgl(?:CreateShader|ShaderSource|CompileShader|LinkProgram|DrawArrays|DrawElements)\s*\(')),
)


def code_only(text):
    return COMMENT.sub(lambda m: '\n' * m[0].count('\n') if m[0].startswith(('//', '/*')) else m[0], text)


def normalize(path, base):
    path = Path(str(path).strip('"').replace('\\', '/'))
    return (base / path).resolve() if not path.is_absolute() else path.resolve()


def inside(path, root):
    return path.is_relative_to(root)


def first_party(path, repo):
    if not inside(path, repo):
        return False
    parts = path.relative_to(repo).parts
    return len(parts) > 2 and parts[0] in ('octaryn-client', 'octaryn-shared', 'octaryn-server', 'octaryn-basegame') and parts[1] == 'Source'


def include_dirs(command, base):
    matches = re.finditer(r'(?:^|\s)(?:-I|/I|-imsvc)(?:"([^"]+)"|([^\s]+))', command)
    return [normalize(m[1] or m[2], base) for m in matches]


def audit(repo, inputs, commands):
    client = repo / 'octaryn-client' / 'Source'
    shaders = repo / 'octaryn-client' / 'Shaders'
    errors = []
    inputs = set(inputs)
    source_files = sorted(p for p in inputs if first_party(p, repo) and p.suffix.lower() in ('.cpp', '.cc', '.c'))
    libraries = sorted(p for p in inputs if p.suffix.lower() in ('.lib', '.a', '.dll', '.so', '.dylib'))
    if not source_files:
        errors.append('configured client closure has no first-party translation units')
    if not any(p.name.lower() in ('slang-rhi.lib', 'libslang-rhi.a') for p in libraries):
        errors.append('configured client does not link the standalone slang-rhi library')
    for library in libraries:
        if re.fullmatch(r'(?:lib)?gfx\.(?:lib|a|dll|so|dylib)', library.name, re.I):
            errors.append(f'configured client links forbidden GFX library: {library}')
    command_map = {normalize(c['file'], Path(c['directory'])): c for c in commands}
    files = {}
    shader_roots = set()
    pending = []
    for source in source_files:
        command = command_map.get(source)
        if not command:
            errors.append(f'no compile command for active source: {source}')
            continue
        pending.append((source, include_dirs(command.get('command', ''), Path(command['directory']))))
    while pending:
        path, directories = pending.pop()
        if path in files:
            continue
        if not path.exists():
            errors.append(f'active source/header is missing: {path}')
            continue
        text = code_only(path.read_text(encoding='utf-8-sig'))
        files[path] = text
        for label, pattern in FORBIDDEN:
            for match in pattern.finditer(text):
                line = text.count('\n', 0, match.start()) + 1
                errors.append(f'{path.relative_to(repo)}:{line}: {label}: {match[0]}')
        for name in INCLUDE.findall(text):
            candidates = [path.parent / name] + [directory / name for directory in directories]
            found = next((p.resolve() for p in candidates if p.is_file()), None)
            if found and first_party(found, repo):
                pending.append((found, directories))
        for name in SHADER_STRING.findall(text):
            if not name.lower().endswith('.slang'):
                continue
            relative = name.removeprefix('octaryn-client/').removeprefix('Shaders/')
            target = (shaders / relative).resolve()
            if not inside(target, shaders) or not target.is_file():
                errors.append(f'active shader path is missing or outside Slang tree: {path}: {name}')
            else:
                shader_roots.add(target)
    joined = '\n'.join(files.values())
    if not re.search(r'\brhi\s*::\s*getRHI\s*\(', joined):
        errors.append('active path does not create/use the standalone RHI singleton')
    if not re.search(r'\brhi\s*::\s*DeviceType\s*::\s*Vulkan\b', joined):
        errors.append('active path does not explicitly select Vulkan through RHI')
    if not shader_roots:
        errors.append('active path has no discoverable Slang shader module paths')
    shader_files = {}
    fsr_vendor = vendor_root(repo)
    fsr_vendor_names = None
    pending_shaders = list(shader_roots)
    while pending_shaders:
        path = pending_shaders.pop()
        if path in shader_files:
            continue
        text = code_only(path.read_text(encoding='utf-8-sig'))
        shader_files[path] = text
        if re.search(r'^\s*#version\b|\bgl_(?:Position|FragColor|FragCoord)\b', text, re.M):
            errors.append(f'GLSL source in active Slang module: {path.relative_to(repo)}')
        for name in INCLUDE.findall(text):
            if path.parent == shaders / 'Fsr2' and name.startswith('ffx_') and name.endswith('.hlsl'):
                try:
                    if fsr_vendor_names is None:
                        fsr_vendor_names = verified_files(fsr_vendor)
                    if name not in fsr_vendor_names:
                        raise ValueError(f'Unlisted FSR2 pass: {name}')
                except (OSError, ValueError, KeyError) as error:
                    errors.append(f'FSR2 pinned vendor include: {error}')
                continue
            found = next((p.resolve() for p in (path.parent / name, shaders / name) if p.is_file()), None)
            if not found or not inside(found, shaders) or found.suffix != '.slang':
                errors.append(f'active shader include is missing or not Slang: {path}: {name}')
            else:
                pending_shaders.append(found)
    return {
        'status': 'PASS' if not errors else 'FAIL',
        'errors': errors,
        'sources': [p.relative_to(repo).as_posix() for p in source_files],
        'headers': [p.relative_to(repo).as_posix() for p in sorted(files) if p not in source_files],
        'shader_roots': [p.relative_to(shaders).as_posix() for p in sorted(shader_roots)],
        'shader_modules': [p.relative_to(shaders).as_posix() for p in sorted(shader_files)],
        'libraries': [p.as_posix() for p in libraries],
        'scope': 'Ninja target transitive inputs, active first-party compile commands and includes; source/graph audit, not GPU execution proof',
    }


def self_test():
    with tempfile.TemporaryDirectory(prefix='octaryn-rhi-audit-') as directory:
        repo = Path(directory).resolve()
        client = repo / 'octaryn-client/Source'
        shaders = repo / 'octaryn-client/Shaders'
        client.mkdir(parents=True)
        shaders.mkdir(parents=True)
        source = client / 'Main.cpp'
        header = client / 'Owner.h'
        shader = shaders / 'Scene.slang'
        base = '#include "Owner.h"\nauto* api=rhi::getRHI(); auto backend=rhi::DeviceType::Vulkan; const char* path="Scene.slang";\n'
        source.write_text(base)
        header.write_text('// gfx::IDevice and example.glsl in provenance comments are not active APIs\n')
        shader.write_text('[shader("compute")] [numthreads(1,1,1)] void main() {}\n')
        (client / 'Dormant.cpp').write_text('gfx::IDevice* old;')
        inputs = [source, repo / 'slang-rhi.lib', repo / 'slang-compiler.lib', repo / 'slang-glslang.dll']
        commands = [{'file': str(source), 'directory': str(repo), 'command': f'clang-cl /I"{client}" {source}'}]
        if audit(repo, inputs, commands)['errors']:
            raise AssertionError('valid standalone path or dormant-source isolation failed')
        cases = {
            'GFX': 'gfx::IDevice* old;',
            'Vulkan call': 'vkCreateDevice(nullptr);',
            'Vulkan type': 'VkImage image;',
            'SDL GPU': 'SDL_GPUDevice* gpu;',
            'GLSL path': 'load("legacy.glsl");',
            'OpenGL call': 'glCompileShader(shader);',
        }
        for name, mutation in cases.items():
            source.write_text(base + mutation)
            if not audit(repo, inputs, commands)['errors']:
                raise AssertionError(f'negative case accepted: {name}')
        source.write_text(base)
        header.write_text('gfx::ICommandQueue* forbidden_header;')
        if not audit(repo, inputs, commands)['errors']:
            raise AssertionError('transitive header API escaped audit')
        header.write_text('')
        for broken in (inputs + [repo / 'gfx.lib'], [p for p in inputs if p.name != 'slang-rhi.lib']):
            if not audit(repo, broken, commands)['errors']:
                raise AssertionError('invalid library graph escaped audit')
        shader.write_text('#include "missing.slang"\n')
        if not audit(repo, inputs, commands)['errors']:
            raise AssertionError('missing shader include escaped audit')
    return {'status': 'PASS', 'positive_cases': 1, 'negative_cases': 10}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--ninja', type=Path)
    parser.add_argument('--target', default='octaryn_client_app')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        print(json.dumps(self_test()))
        return 0
    repo = args.repo.resolve()
    build = (args.build_dir or repo / 'build/release-windows/cmake').resolve()
    ninja = args.ninja or repo / 'build/dependencies/tools/ninja/ninja.exe'
    result = subprocess.run([str(ninja), '-C', str(build), '-t', 'inputs', args.target], capture_output=True, text=True)
    if result.returncode:
        print(result.stderr, file=sys.stderr)
        return 1
    compilation = subprocess.run([str(ninja), '-C', str(build), '-t', 'compdb-targets', args.target], capture_output=True, text=True)
    if compilation.returncode:
        print(compilation.stderr, file=sys.stderr)
        return 1
    commands = json.loads(compilation.stdout)
    report = audit(repo, [normalize(line, build) for line in result.stdout.splitlines() if line.strip()], commands)
    report['target'] = args.target
    report['build_dir'] = str(build)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"render_pipeline_audit={report['status']} sources={len(report['sources'])} headers={len(report['headers'])} shaders={len(report['shader_modules'])}")
    for error in report['errors']:
        print(error, file=sys.stderr)
    return 0 if not report['errors'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
