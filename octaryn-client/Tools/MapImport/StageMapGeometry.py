"""Validate and stage source-matched, checksummed index-only map LOD assets."""
from pathlib import Path
import argparse
import hashlib
import shutil
import struct


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def stage(cache_root, source_maps, bundle_maps):
    bundle_maps.mkdir(parents=True, exist_ok=True)
    for source in sorted(source_maps.glob('*.glb')):
        payload = cache_root / (source.name + '.lods')
        receipt = payload.with_suffix(payload.suffix + '.sha256')
        if not payload.is_file() or not 96 <= payload.stat().st_size <= 512 * 1024 * 1024:
            raise ValueError(f'required LOD cook missing or oversized: {payload}')
        if not receipt.is_file() or receipt.stat().st_size != 64 or receipt.read_text() != digest(payload):
            raise ValueError(f'LOD checksum mismatch: {payload}')
        with payload.open('rb') as stream:
            header = stream.read(96)
        magic, version, vertices, indices, primitives, lod_indices, reserved0, reserved1 = struct.unpack('<8I', header[:32])
        if magic != 0x444f4c4d or version != 2 or reserved0 or reserved1 or not vertices or not indices or not primitives:
            raise ValueError(f'LOD metadata mismatch: {payload}')
        if header[32:].decode('ascii') != digest(source):
            raise ValueError(f'LOD source mismatch: {source}')
        if payload.stat().st_size != 96 + primitives * 24 + lod_indices * 4:
            raise ValueError(f'LOD payload size mismatch: {payload}')
        shutil.copy2(payload, bundle_maps / payload.name)
        shutil.copy2(receipt, bundle_maps / receipt.name)
        print(f'map_geometry_stage map={source.name} lod_indices={lod_indices} verified=1')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache-root', required=True, type=Path)
    parser.add_argument('--source-maps', required=True, type=Path)
    parser.add_argument('--bundle-maps', required=True, type=Path)
    args = parser.parse_args()
    stage(args.cache_root, args.source_maps, args.bundle_maps)


if __name__ == '__main__':
    main()
