"""Create a fresh prototype bundle without modifying the ordinary client bundle.

Run only after the native build completes. Managed code, server binaries, maps,
atlases and other content are copied from the supplied bundle, not rebuilt.
This stages files and records provenance; it does not qualify runtime behavior.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/validation'))
from fsr2_vendor import vendor_root, verified_files
from validate_client_shader_bundle import validate as validate_shaders

RUNTIME = ('slang.dll', 'slang-compiler.dll', 'slang-rt.dll', 'slang-glslang.dll',
           'dxcompiler.dll', 'dxil.dll', 'nethost.dll')
TRANSIENT = {'logs', 'saves', 'cache', 'caches', 'shader-cache'}
MANIFEST = 'block-transport-bundle-manifest.json'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def direct_path(path):
    path = Path(os.path.abspath(path))
    require(path.resolve() == path and not path.is_symlink(), f'Redirected path: {path}')
    for part in (path, *path.parents):
        require(not part.is_symlink() and not (hasattr(part, 'is_junction') and part.is_junction()),
                f'Linked path: {part}')
    return path


def files(root, skip=None):
    require(root.is_dir(), f'Missing input directory: {root}')
    for parent, directories, names in os.walk(root, followlinks=False):
        current = Path(parent)
        for name in list(directories):
            child = current / name
            relative = child.relative_to(root)
            if skip and skip(relative):
                directories.remove(name)
                continue
            direct_path(child)
        for name in sorted(names):
            child = current / name
            relative = child.relative_to(root)
            if skip and skip(relative):
                continue
            direct_path(child)
            require(child.is_file(), f'Non-file payload: {child}')
            yield relative, child


def aggregate(records):
    entries = [(record['path'], record['sha256']) for record in records]
    return hashlib.sha256(json.dumps(sorted(entries), separators=(',', ':')).encode()).hexdigest()


def plan(source, destination, native, jobs, shaders, ui):
    require(not destination.exists(), f'Destination already exists; choose a fresh directory: {destination}')
    require(source != destination and not destination.is_relative_to(source)
            and not source.is_relative_to(destination), 'Source and destination must be separate trees')
    require(destination.parent.is_dir(), f'Destination parent must already exist: {destination.parent}')
    require(destination.parent == ROOT / 'build/release-windows/client',
            'Prototype destination must be a direct child of build/release-windows/client')
    require(destination.name.startswith('block-transport-bundle'),
            'Prototype directory name must start with block-transport-bundle')
    require(all((source / name).is_file() for name in ('Octaryn.Client.exe', 'Octaryn.Client.dll',
            'Octaryn.Client.runtimeconfig.json', 'server/Octaryn.Server.dll')),
            'Source must contain an ordinary complete client/server bundle')
    overlays = {'Octaryn.Client.exe': native / 'Octaryn.Client.exe',
                'octaryn_client_managed_bridge.dll': native / 'octaryn_client_managed_bridge.dll',
                'octaryn_native_jobs.dll': jobs}
    for path in overlays.values():
        direct_path(path)
        require(path.is_file() and path.stat().st_size > 0, f'Missing native build output: {path}')
    for name in RUNTIME:
        left, right = source / name, native / name
        require(left.is_file() and right.is_file(), f'Missing runtime dependency: {name}')
        require(digest(left) == digest(right), f'Bundle/runtime mismatch for {name}; qualify the changed dependency first')

    def omitted(relative):
        parts = tuple(part.casefold() for part in relative.parts)
        return (parts[:2] in (('client', 'shaders'), ('assets', 'ui')) or
                any(part in TRANSIENT for part in parts) or
                relative.as_posix() in {*overlays, MANIFEST})

    entries = [(relative.as_posix(), path, 'retained_bundle') for relative, path in files(source, omitted)]
    entries += [(name, path, 'native_build') for name, path in overlays.items()]
    for relative, path in files(shaders):
        require(path.suffix == '.slang', f'Non-Slang file in active shader tree: {path}')
        entries.append(('Client/Shaders/' + relative.as_posix(), path, 'active_shader'))
    vendor = direct_path(vendor_root(ROOT))
    vendor_files = verified_files(vendor)
    entries += [('Client/Shaders/Fsr2/Vendor/' + name, vendor / name, 'pinned_fsr2_vendor')
                for name in sorted(vendor_files)]
    entries += [('Assets/Ui/' + relative.as_posix(), path, 'active_basegame_ui')
                for relative, path in files(ui) if path.name != '.gitkeep']
    names = [name.casefold() for name, _, _ in entries]
    require(len(names) == len(set(names)), 'Overlapping payload destinations')
    for _, path, _ in entries:
        direct_path(path)
        require(not path.is_relative_to(destination), 'Input is inside the prototype destination')
    return sorted(entries)


def copy_verified(source, destination):
    before = source.stat()
    expected = digest(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with source.open('rb') as incoming, destination.open('xb') as outgoing:
        shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
    after = source.stat()
    require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns)
            and digest(destination) == expected and digest(source) == expected,
            f'Input changed or copy verification failed: {source}')
    return {'sha256': expected, 'bytes': after.st_size, 'source_mtime_ns': after.st_mtime_ns}


def stage(args):
    source, destination = direct_path(args.source_bundle), direct_path(args.destination)
    native, jobs = direct_path(args.native_dir), direct_path(args.native_jobs)
    shaders, ui = direct_path(ROOT / 'octaryn-client/Shaders'), direct_path(ROOT / 'octaryn-basegame/Assets/Ui')
    build_log = direct_path(args.build_log)
    require(build_log.is_file() and build_log.stat().st_size > 0, f'Missing native build evidence: {build_log}')
    evidence = {'native_build': {'path': str(build_log), 'sha256': digest(build_log)}}
    if args.source_evidence:
        reference = direct_path(args.source_evidence)
        require(reference.is_file(), f'Missing source bundle evidence: {reference}')
        evidence['source_bundle'] = {'path': str(reference), 'sha256': digest(reference)}
    entries = plan(source, destination, native, jobs, shaders, ui)
    destination.mkdir()  # Exclusive creation; existing bundles are never replaced.
    records = []
    manifest = {'status': 'incomplete', 'created_utc': datetime.now(timezone.utc).isoformat(),
                'source_bundle': str(source), 'destination': str(destination), 'evidence': evidence,
                'limitations': ['No native or managed build is performed by this tool.',
                    'Managed code, complete server payload, maps, atlases and non-UI assets are retained from source_bundle.',
                    'Only client EXE, managed bridge, native jobs, first-party shaders, pinned FSR2 includes and basegame UI are refreshed.',
                    'Build/evidence files are recorded, not interpreted as proof of successful build or runtime qualification.',
                    'No graphics, gameplay, networking or platform qualification is established by staging.'],
                'excluded': ['Client/Shaders (replaced)', 'Assets/Ui (replaced)',
                             'native overlay files (replaced)', 'logs/saves/cache directories'],
                'files': records}
    manifest_path = destination / MANIFEST
    try:
        for name, path, role in entries:
            record = copy_verified(path, destination / name)
            records.append({'path': name, 'source': str(path), 'role': role, **record})
        errors = validate_shaders(shaders, destination / 'Client/Shaders')
        require(not errors, '\n'.join(errors))
        retained = [record for record in records if record['role'] == 'retained_bundle']
        # Check again after all copies so input changes during staging fail closed.
        require(all(digest(Path(record['source'])) == record['sha256'] for record in records),
                'An input changed during staging; incomplete destination retained for inspection')
        manifest.update(status='staged', payload_sha256=aggregate(records), files_count=len(records),
                        retained_payload_sha256=aggregate(retained), retained_files=len(retained),
                        client_sha256=next(record['sha256'] for record in records if record['path'] == 'Octaryn.Client.exe'),
                        shader_bundle_validation='passed')
    except Exception as error:
        manifest['error'] = str(error)
        raise
    finally:
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f"block_transport_bundle=staged files={len(records)} client_sha256={manifest['client_sha256']} destination={destination}")
    print(f'provenance={manifest_path}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    client = ROOT / 'build/release-windows/client'
    parser.add_argument('--source-bundle', type=Path, default=client / 'bundle')
    parser.add_argument('--destination', type=Path, default=client / 'block-transport-bundle')
    parser.add_argument('--native-dir', type=Path, default=client / 'native/bin')
    parser.add_argument('--native-jobs', type=Path, default=ROOT / 'build/release-windows/shared/native/bin/octaryn_native_jobs.dll')
    parser.add_argument('--build-log', type=Path, required=True, help='Evidence from the completed native build; stored with its hash')
    parser.add_argument('--source-evidence', type=Path, help='Optional source bundle qualification evidence, recorded without reinterpreting it')
    args = parser.parse_args()
    try:
        stage(args)
    except (OSError, ValueError) as error:
        print(f'block_transport_bundle=failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
