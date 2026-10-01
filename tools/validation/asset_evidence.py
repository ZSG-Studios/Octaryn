"""Cheap cooked-asset identities; payload validation remains the runtime reader's job."""
from pathlib import Path
import hashlib
import re
import json


def receipt_identity(paths, root):
    aggregate = hashlib.sha256()
    count = 0
    for path in sorted(paths, key=lambda item: item.relative_to(root).as_posix()):
        value = path.read_text(encoding='ascii')
        if not re.fullmatch('[0-9a-f]{64}', value):
            raise ValueError(f'invalid asset digest receipt: {path}')
        aggregate.update(path.relative_to(root).as_posix().encode('utf-8') + b'\0' + value.encode('ascii') + b'\n')
        count += 1
    return dict(count=count, sha256=aggregate.hexdigest(), payloads_rehashed=False)


def cooked_asset_identity(map_path, manifest_path, manifest):
    root = manifest_path.parent
    cache = root / manifest['texture_cache'] if manifest.get('texture_cache') else Path(str(map_path) + '.textures')
    metadata = cache / 'map-texture-cook.json'
    maps = [root / tile for tile in manifest.get('tile_files', [])] or [map_path]
    lod_receipts = [Path(str(path) + '.lods.sha256') for path in maps]
    lod_receipts = [path for path in lod_receipts if path.is_file()]
    result = dict(schema_version=1, texture_cache=str(cache), texture_cache_present=cache.is_dir(),
                  texture_receipts=receipt_identity(cache.glob('*.dds.sha256'), cache),
                  lod_receipts=receipt_identity(lod_receipts, root))
    if metadata.is_file():
        result['texture_manifest_sha256'] = hashlib.sha256(metadata.read_bytes()).hexdigest()
    return result


def compare_recorded_asset_identities(assets):
    coverage = {}
    for key in ('manifest_sha256', 'cooked_metadata', 'cooked_identity'):
        values = []
        for asset in assets:
            if key not in asset:
                continue
            value = asset[key]
            if key == 'cooked_identity':
                value = {name: entry for name, entry in value.items() if name != 'texture_cache'}
            if key == 'cooked_metadata':
                value = {name.replace('\\', '/'): entry for name, entry in value.items()}
            values.append(json.dumps(value, sort_keys=True, separators=(',', ':')))
        if len(set(values)) > 1:
            raise ValueError(f'Recorded asset {key} identities differ')
        coverage[key + '_recorded_runs'] = len(values)
    coverage['total_runs'] = len(assets)
    coverage['complete_cooked_receipt_coverage'] = bool(assets) and all(
        bool(asset.get('cooked_identity', {}).get('texture_receipts')) for asset in assets)
    coverage['note'] = ('GLB hash and parsed manifest are compared separately for all runs; cook metadata and '
                        'aggregate DDS/LOD receipt identities are compared wherever recorded. Missing historical '
                        'identities are not reconstructed; receipt identities do not rehash payloads.')
    return coverage
