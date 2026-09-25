"""Stage only complete, source-matched offline map texture caches into a bundle."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil

CACHE_VERSION = 2


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def stage(cache_root: Path, source_maps: Path, bundle_maps: Path) -> int:
    copied = 0
    for source in sorted(source_maps.glob('*.glb')):
        cache = cache_root / (source.name + '.textures')
        manifest = cache / 'map-texture-cook.json'
        if not manifest.is_file():
            continue
        if manifest.stat().st_size > 1024 * 1024:
            raise ValueError(f'oversized texture manifest: {manifest}')
        document = json.loads(manifest.read_text(encoding='utf-8'))
        if document.get('status') != 'complete' or document.get('version') != CACHE_VERSION:
            print(f'map_texture_stage skipped={source.name} reason=pilot_or_version')
            continue
        if document.get('map_sha256') != digest(source):
            print(f'map_texture_stage skipped={source.name} reason=source_hash_changed')
            continue
        keys = document.get('files')
        if not isinstance(keys, list) or len(keys) > 10000 or any(
                not isinstance(key, str) or not re.fullmatch(r'[0-9a-f]{64}', key) for key in keys):
            raise ValueError('invalid texture cache key list')
        if len(set(keys)) != len(keys):
            raise ValueError('duplicate texture cache keys')
        payloads = []
        for key in keys:
            dds = cache / (key + '.dds')
            checksum = cache / (key + '.dds.sha256')
            if not dds.is_file() or not 148 <= dds.stat().st_size <= 86 * 1024 * 1024:
                raise ValueError(f'missing or oversized cache payload: {dds}')
            if not checksum.is_file() or checksum.stat().st_size != 64:
                raise ValueError(f'missing cache integrity receipt: {dds}')
            if checksum.read_text(encoding='ascii') != digest(dds):
                raise ValueError(f'cache digest mismatch: {dds}')
            payloads.extend((dds, checksum))
        destination = bundle_maps / cache.name
        destination.mkdir(parents=True, exist_ok=True)
        for payload in payloads:
            shutil.copy2(payload, destination / payload.name)
        shutil.copy2(manifest, destination / manifest.name)
        copied += len(keys)
        print(f'map_texture_stage map={source.name} variants={len(keys)} verified=1')
    return copied


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache-root', required=True, type=Path)
    parser.add_argument('--source-maps', required=True, type=Path)
    parser.add_argument('--bundle-maps', required=True, type=Path)
    args = parser.parse_args()
    stage(args.cache_root, args.source_maps, args.bundle_maps)


if __name__ == '__main__':
    main()
