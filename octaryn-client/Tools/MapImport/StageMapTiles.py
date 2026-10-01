"""Validate and stage HQ200 tiles while sharing the native reference texture cache."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import shutil
import struct


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def document(path):
    if not path.is_file() or not 0 < path.stat().st_size <= 1024 * 1024:
        raise ValueError(f'missing or oversized tile metadata: {path}')
    return json.loads(path.read_text(encoding='utf-8'))


def local(root, relative):
    path = Path(relative)
    if path.is_absolute() or path.drive or '..' in path.parts:
        raise ValueError(f'invalid tile payload path: {relative}')
    resolved = (root / path).resolve()
    if not resolved.is_relative_to(root.resolve()):
        raise ValueError(f'tile payload escapes owner: {relative}')
    return resolved


def stage(source_tiles, source_map, bundle_maps):
    manifest = document(source_tiles / 'map.json')
    cooked = document(source_tiles / 'cook.json')
    verified = document(source_tiles / 'verify.json')
    source_hash = digest(source_map)
    if cooked.get('source_sha256') != source_hash or verified.get('source_sha256') != source_hash:
        raise ValueError('tile cook or verification source mismatch')
    names = manifest.get('tile_files')
    if not isinstance(names, list) or not 1 <= len(names) <= 4096 or len(set(names)) != len(names):
        raise ValueError('invalid tile file list')
    if manifest.get('version') != 1 or len(manifest.get('tiles', [])) != len(names):
        raise ValueError('tile bounds do not match files')
    for bounds in manifest['tiles']:
        if len(bounds) != 6 or not all(math.isfinite(v) for v in bounds) or any(bounds[i] > bounds[i + 3] for i in range(3)):
            raise ValueError('invalid tile bounds')
    if verified.get('version') != 1 or verified.get('scope') != 'positions_uv_colors_winding_dual64':
        raise ValueError('missing native tile conservation verification')
    if set(verified.get('files', {})) != set(names) or manifest.get('map') not in names:
        raise ValueError('tile verification file coverage mismatch')
    reference_cache = bundle_maps / (source_map.name + '.textures')
    textures = document(reference_cache / 'map-texture-cook.json')
    if textures.get('version') != 3 or textures.get('status') != 'complete' or textures.get('map_sha256') != source_hash:
        raise ValueError('required shared reference texture cache is not source-matched')
    payloads = []
    images = {}
    total = maximum = 0
    for name in names:
        glb = local(source_tiles, name)
        if not glb.is_file() or not 28 <= glb.stat().st_size <= 16 * 1024 * 1024:
            raise ValueError('tile GLB exceeds bounded source')
        glb_hash = digest(glb)
        if verified['files'][name] != glb_hash:
            raise ValueError('tile GLB changed after native verification')
        with glb.open('rb') as stream:
            magic, version, length, json_bytes, chunk = struct.unpack('<5I', stream.read(20))
            if (magic, version, length, chunk) != (0x46546c67, 2, glb.stat().st_size, 0x4e4f534a):
                raise ValueError('invalid tile GLB header')
            if not 0 < json_bytes <= length - 28:
                raise ValueError('invalid tile GLB JSON length')
            tile = json.loads(stream.read(json_bytes))
            binary_bytes, binary_kind = struct.unpack('<2I', stream.read(8))
            if binary_kind != 0x004e4942 or binary_bytes + json_bytes + 28 != length:
                raise ValueError('invalid tile binary chunk')
        buffers = tile.get('buffers', [])
        if len(buffers) != 1 or 'uri' in buffers[0] or buffers[0]['byteLength'] != binary_bytes:
            raise ValueError('tile geometry must be embedded and bounded')
        accessors = tile['accessors']
        if any(a['count'] > 49152 for a in accessors):
            raise ValueError('tile accessor exceeds preparation bound')
        primitives = [p for mesh in tile['meshes'] for p in mesh['primitives']]
        indices = [accessors[p['indices']]['count'] for p in primitives]
        if any(p.get('mode', 4) != 4 for p in primitives) or any(n % 3 for n in indices):
            raise ValueError('tile topology is not independent triangles')
        triangles = sum(indices) // 3
        if not 0 < triangles <= 16384 or not 0 < len(primitives) <= 2048:
            raise ValueError('tile exceeds HQ200 geometry work bound')
        total += triangles
        maximum = max(maximum, triangles)
        encoded = 0
        for image in tile.get('images', []):
            path = (glb.parent / image['uri']).resolve()
            if not path.is_relative_to((source_tiles / 'images').resolve()) or not path.is_file():
                raise ValueError('tile image escapes shared image owner or is missing')
            encoded += path.stat().st_size
            if path not in images:
                if digest(path) != path.stem:
                    raise ValueError('shared source image digest mismatch')
                images[path] = path.relative_to(source_tiles.resolve())
        if encoded > 64 * 1024 * 1024:
            raise ValueError('tile encoded images exceed preparation bound')
        lod = glb.with_suffix('.glb.lods')
        receipt = lod.with_suffix('.lods.sha256')
        if not lod.is_file() or not 96 <= lod.stat().st_size <= 512 * 1024 * 1024:
            raise ValueError('required tile geometry cook missing')
        if not receipt.is_file() or receipt.stat().st_size != 64 or receipt.read_text() != digest(lod):
            raise ValueError('tile LOD integrity mismatch')
        with lod.open('rb') as stream:
            header = stream.read(96)
        values = struct.unpack('<8I', header[:32])
        if values[0:2] != (0x444f4c4d, 1) or values[6:] != (0, 0) or header[32:].decode('ascii') != glb_hash:
            raise ValueError('tile LOD source metadata mismatch')
        if lod.stat().st_size != 96 + values[4] * 24 + values[5] * 4:
            raise ValueError('tile LOD size mismatch')
        payloads.extend((glb, lod, receipt))
    if cooked.get('version') != 1 or total != cooked.get('triangles') or total != verified.get('triangles') or maximum != verified.get('maximum_tile_triangles'):
        raise ValueError('tile conservation metadata mismatch')
    keys = textures.get('files', [])
    if not keys or len(set(keys)) != len(keys):
        raise ValueError('shared texture inventory missing or duplicated')
    for key in keys:
        if not isinstance(key, str) or len(key) != 64 or any(c not in '0123456789abcdef' for c in key):
            raise ValueError('invalid shared texture key')
        dds = reference_cache / (key + '.dds')
        receipt = dds.with_suffix('.dds.sha256')
        if not dds.is_file() or not 148 <= dds.stat().st_size <= 86 * 1024 * 1024:
            raise ValueError('required shared cooked texture missing')
        if not receipt.is_file() or receipt.stat().st_size != 64 or receipt.read_text() != digest(dds):
            raise ValueError('shared texture integrity mismatch')
    destination = bundle_maps / 'hq200'
    for path in payloads + list(images):
        target = destination / path.relative_to(source_tiles.resolve())
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    for name in ('cook.json', 'verify.json'):
        shutil.copy2(source_tiles / name, destination / name)
    manifest['map'] = 'hq200/' + manifest['map']
    manifest['tile_files'] = ['hq200/' + name for name in names]
    manifest['texture_cache'] = source_map.name + '.textures'
    (bundle_maps / 'hq200.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'map_tile_stage tiles={len(names)} triangles={total} maximum={maximum} images={len(images)} shared_texture_variants={len(keys)} verified=1')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-tiles', type=Path, required=True)
    parser.add_argument('--source-map', type=Path, required=True)
    parser.add_argument('--bundle-maps', type=Path, required=True)
    args = parser.parse_args()
    stage(args.source_tiles.resolve(), args.source_map.resolve(), args.bundle_maps.resolve())


if __name__ == '__main__':
    main()
