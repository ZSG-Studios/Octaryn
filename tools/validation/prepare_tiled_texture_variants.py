"""Stage matched tiled lossless/BC7 manifests without cooking or changing defaults."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from prepare_map_texture_variants import ROOT, BUILD, digest, link, stage_cache


def inside(root, name):
    path = (root / name).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError(f'Missing or escaping tiled dependency: {name}')
    return path


def geometry_files(manifest):
    root = manifest.parent.resolve()
    document = json.loads(manifest.read_text())
    names = document.get('tile_files', [])
    if not names or len(names) != len(document.get('tiles', [])) or len(set(names)) != len(names):
        raise ValueError('Invalid tiled manifest')
    if document.get('map') not in names:
        raise ValueError('Primary map must be a listed tile')
    files = {}
    for name in names:
        path = inside(root, name)
        with path.open('rb') as stream:
            header = stream.read(20)
            if len(header) != 20:
                raise ValueError('Truncated GLB header')
            magic, version, size, count, kind = struct.unpack('<5I', header)
            if magic != 0x46546c67 or version != 2 or kind != 0x4e4f534a or size != path.stat().st_size or count > 16*1024*1024:
                raise ValueError('Invalid GLB metadata')
            gltf = json.loads(stream.read(count))
        files[path.relative_to(root).as_posix()] = path
        for record in gltf.get('images', []) + gltf.get('buffers', []):
            if 'uri' not in record:
                continue
            dependency = inside(root, path.parent / record['uri'])
            files[dependency.relative_to(root).as_posix()] = dependency
        lod = Path(str(path) + '.lods')
        receipt = Path(str(lod) + '.sha256')
        if not lod.is_file() or not receipt.is_file() or receipt.read_text() != digest(lod):
            raise ValueError(f'Missing or corrupt tile LOD: {lod}')
        with lod.open('rb') as stream:
            header = stream.read(96)
        if len(header) != 96 or header[32:].decode('ascii') != digest(path):
            raise ValueError(f'Stale tile LOD: {lod}')
        files[lod.relative_to(root).as_posix()] = lod
        files[receipt.relative_to(root).as_posix()] = receipt
    return document, files


def prepare(manifest, source, reference, candidate, output):
    paths = [path.resolve() for path in (manifest, source, reference, candidate, output)]
    manifest, source, reference, candidate, output = paths
    if reference == candidate or any(path.is_relative_to(output) or output.is_relative_to(path)
                                     for path in (manifest.parent, reference, candidate)):
        raise ValueError('Variants must be separate from source and cache directories')
    source_hash = digest(source)
    view, files = geometry_files(manifest)
    geometry = [dict(path=name, bytes=path.stat().st_size, sha256=digest(path)) for name, path in sorted(files.items())]
    geometry_hash = hashlib.sha256(json.dumps(geometry, sort_keys=True).encode()).hexdigest()
    variants = {}
    keys = None
    for name, cache, codec in [('lossless', reference, 'rgba8-lossless'),
                                ('bc7', candidate, 'bc7-opaque-color-uber4')]:
        metadata = json.loads((cache / 'map-texture-cook.json').read_text())
        if metadata.get('codec') != codec or (keys is not None and set(metadata['files']) != keys):
            raise ValueError('Codec or texture-key set mismatch')
        keys = set(metadata['files'])
        destination = output / name
        identity = stage_cache(cache, destination / 'textures', source_hash)
        for relative, path in files.items():
            link(path, destination / relative)
        variant_view = dict(view, texture_cache='textures')
        variant_manifest = destination / 'map.json'
        variant_manifest.write_text(json.dumps(variant_view, indent=2) + '\n')
        variants[name] = dict(manifest=str(variant_manifest), manifest_sha256=digest(variant_manifest),
                              geometry_identity=geometry_hash, **identity)
    if variants['lossless']['manifest_sha256'] != variants['bc7']['manifest_sha256']:
        raise ValueError('Variant views or geometry differ')
    result = dict(schema_version=1, source_manifest=str(manifest), source_manifest_sha256=digest(manifest),
                  source_sha256=source_hash, tile_count=len(view['tile_files']),
                  geometry_files=geometry, variants=variants,
                  allowed_difference='Cooked texture payload/receipts only; geometry, image sources, LOD, bounds and view identical.',
                  qualification='Prepared assets only; runtime quality and three matched repetitions remain required.',
                  file_policy='Payloads are hardlinked when possible. Never modify staged payload files in place.')
    (output / 'variants.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=ROOT / 'octaryn-client/Assets/Maps/main.glb')
    parser.add_argument('--reference', type=Path, default=BUILD / 'client/map-textures/main.glb.textures')
    parser.add_argument('--bc7-cache', type=Path, default=BUILD / 'client/map-textures-bc7-opaque/main.glb.textures')
    parser.add_argument('--output', type=Path, default=BUILD / 'client/map-variants-hq200')
    args = parser.parse_args()
    result = prepare(args.manifest, args.source, args.reference, args.bc7_cache, args.output)
    print(json.dumps({key: value for key, value in result.items() if key != 'geometry_files'}, indent=2))


if __name__ == '__main__':
    main()
