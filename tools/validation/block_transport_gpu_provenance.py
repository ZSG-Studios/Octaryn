"""Bounded shader snapshots for the production and source-path GPU fixture roots."""
import hashlib
import json


def shader_snapshot(root, fixture):
    directories = {
        'staged_client': fixture / 'Client/Shaders',
        'source_client': root / 'octaryn-client/Shaders',
        'source_probes': root / 'tools/Source/ClientWorldMeshProbe',
    }
    trees = {}
    for name, directory in directories.items():
        if not directory.is_dir():
            raise FileNotFoundError(f'Missing shader provenance directory: {directory}')
        extensions = {'.slang'} if name == 'source_probes' else {'.slang', '.hlsl', '.hlsli', '.h', '.inc'}
        files = {path.relative_to(directory).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in sorted(directory.rglob('*')) if path.is_file() and path.suffix.lower() in extensions}
        if not files:
            raise RuntimeError(f'Empty shader provenance directory: {directory}')
        digest = hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()
        trees[name] = dict(path=str(directory.resolve()), sha256=digest, files=files)
    digest = hashlib.sha256(json.dumps({key: value['sha256'] for key, value in trees.items()},
                                      sort_keys=True).encode()).hexdigest()
    return dict(sha256=digest, trees=trees)


def verify_shader_snapshot(root, fixture, expected):
    current = shader_snapshot(root, fixture)
    if current != expected:
        changed = [name for name in expected['trees'] if current['trees'][name] != expected['trees'][name]]
        raise RuntimeError('Shader sources changed during grouped qualification: ' + ', '.join(changed))
