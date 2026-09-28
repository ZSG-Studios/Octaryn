"""Prepare isolated lossless/BC7 map manifests for matched GPU captures."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'build/release-windows'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def link(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        if os.path.samefile(source, destination):
            return
        raise ValueError(f'variant destination already exists: {destination}')
    try:
        os.link(source, destination)
    except OSError:
        shutil.copy2(source, destination)


def stage_cache(cache, output, source_hash):
    manifest = cache / 'map-texture-cook.json'
    document = json.loads(manifest.read_text())
    keys = document.get('files')
    if (document.get('version') != 3 or document.get('status') != 'complete'
            or document.get('map_sha256') != source_hash or not isinstance(keys, list)
            or len(keys) > 10000 or len(keys) != len(set(keys))
            or any(not isinstance(key, str) or not re.fullmatch('[0-9a-f]{64}', key) for key in keys)):
        raise ValueError(f'invalid or stale cooked manifest: {manifest}')
    payload_bytes = 0
    payload_identity = hashlib.sha256()
    for key in keys:
        dds, checksum = cache / (key + '.dds'), cache / (key + '.dds.sha256')
        checksum_text = checksum.read_text()
        if not 148 <= dds.stat().st_size <= 86 * 1024 * 1024 or checksum_text != digest(dds):
            raise ValueError(f'cooked payload integrity failed: {dds}')
        payload_identity.update((key + checksum_text).encode('ascii'))
        payload_bytes += dds.stat().st_size - 148
        for file in (dds, checksum):
            link(file, output / file.name)
    link(manifest, output / manifest.name)
    return dict(codec=document['codec'], source_sha256=source_hash,
                texture_manifest_sha256=digest(manifest),
                texture_payload_identity=payload_identity.hexdigest(),
                variants=len(keys), texture_payload_bytes=payload_bytes)


def stage(source, cache, output, source_hash, view, lod):
    identity = stage_cache(cache, output / (source.name + '.textures'), source_hash)
    link(source, output / source.name)
    for suffix in ('', '.sha256'):
        link(Path(str(lod) + suffix), output / (source.name + '.lods' + suffix))
    (output / 'map.json').write_text(json.dumps(dict(view, map=source.name), indent=2) + '\n')
    return dict(manifest=str(output / 'map.json'), **identity)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'octaryn-client/Assets/Maps/main.glb')
    parser.add_argument('--view', type=Path, default=ROOT / 'octaryn-client/Assets/Maps/map.json')
    parser.add_argument('--reference', type=Path, default=BUILD / 'client/map-textures/main.glb.textures')
    parser.add_argument('--bc7-cache', type=Path, default=BUILD / 'client/map-textures-bc7-opaque/main.glb.textures')
    parser.add_argument('--lod', type=Path, default=BUILD / 'client/map-geometry/main.glb.lods')
    parser.add_argument('--output', type=Path, default=BUILD / 'client/map-variants-opaque')
    parser.add_argument('--cooker', type=Path, default=BUILD / 'tools/map-texture-cache/map_texture_cook.exe')
    parser.add_argument('--cook', action='store_true', help='Run the expensive BC7 cook before staging')
    parser.add_argument('--jobs', type=int, choices=range(1, 9), default=4)
    parser.add_argument('--report', type=Path, default=ROOT / 'logs/client/performance-bc7-opaque-textures.csv')
    args = parser.parse_args()
    source, reference, candidate, output = (path.resolve() for path in (args.source, args.reference, args.bc7_cache, args.output))
    if reference == candidate or output in (reference, candidate) or reference in output.parents or candidate in output.parents:
        parser.error('reference, BC7 cache, and variants must be separate directories')
    started = time.perf_counter()
    flags = getattr(subprocess, 'BELOW_NORMAL_PRIORITY_CLASS', 0)
    if args.cook:
        subprocess.run([str(args.cooker.resolve()), str(source), str(candidate), '--bc7', '--jobs', str(args.jobs)], check=True, creationflags=flags)
    view = json.loads(args.view.read_text())
    source_hash = digest(source)
    with args.lod.open('rb') as stream:
        lod_header = stream.read(96)
    if (len(lod_header) != 96 or lod_header[32:].decode('ascii') != source_hash
            or Path(str(args.lod) + '.sha256').read_text() != digest(args.lod)):
        raise ValueError('LOD reference does not match the map source or its checksum')
    result = {'version': 1, 'variants': {
        name: stage(source, cache, output / name, source_hash, view, args.lod.resolve())
        for name, cache in (('lossless', reference), ('bc7', candidate))}}
    report = args.report.resolve()
    subprocess.run([str(args.cooker.resolve()), '--compare', str(reference), str(candidate), str(report)], check=True, creationflags=flags)
    result.update(elapsed_seconds=time.perf_counter() - started, cpu_quality_report=str(report))
    (output / 'variants.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
