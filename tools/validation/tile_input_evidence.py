"""Content identities for tiled rendering captures, including shared cook metadata."""
import hashlib
import json
from pathlib import Path


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def record_inputs(path, destination):
    path = path.resolve()
    manifest = json.loads(path.read_text(encoding='utf-8'))
    files = manifest.get('tile_files', [])
    bounds = manifest.get('tiles', [])
    if not files or len(files) != len(bounds):
        raise ValueError('Capture requires a nonempty tiled manifest')
    identities = []
    paths = {path}
    for name in files:
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Tile paths must stay within the manifest directory')
        source = path.parent / relative
        if not source.is_file():
            raise ValueError(f'Missing tile: {source}')
        paths.add(source.resolve())
        for suffix in ('.lods', '.lods.sha256'):
            companion = Path(str(source) + suffix)
            if companion.is_file():
                paths.add(companion.resolve())
    metadata = []
    if manifest.get('texture_cache'):
        relative = Path(manifest['texture_cache'])
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Texture cache must stay within the manifest directory')
        cache = path.parent / relative
        for pattern in ('*.json', '*.sha256'):
            metadata.extend(cache.rglob(pattern))
    for name in ('cook.json', 'map-texture-cook.json'):
        if (path.parent / name).is_file():
            metadata.append(path.parent / name)
    paths.update(p.resolve() for p in metadata)
    for source in sorted(paths):
        identities.append(dict(path=str(source), bytes=source.stat().st_size, sha256=digest(source)))
    result = dict(manifest_path=str(path), manifest=manifest, tile_count=len(files),
                  inputs=identities, shared_cook_metadata_files=len(set(metadata)),
                  loading_cache_state='uncontrolled; identity hashing may warm file cache')
    destination.write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def outside_manifest(row, bounds):
    x, z = float(row['eye_x']), float(row['eye_z'])
    return not any(b[0] <= x <= b[3] and b[2] <= z <= b[5] for b in bounds)
