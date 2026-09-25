"""Create a GLB changing only the two known Bistro foliage materials to MASK."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

from BistroAlpha import BISTRO_CUTOUT_MATERIALS


def read_document(stream):
    magic, version, size = struct.unpack('<III', stream.read(12))
    length, kind = struct.unpack('<II', stream.read(8))
    if magic != 0x46546c67 or version != 2 or kind != 0x4e4f534a:
        raise ValueError('Expected GLB 2.0 JSON first chunk')
    return json.loads(stream.read(length)), size


def hash_file(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def patch(source, output):
    if output.exists() or source.resolve() == output.resolve():
        raise ValueError('Output must be a new file; source is never overwritten')
    with source.open('rb') as stream:
        document, size = read_document(stream)
        if size != source.stat().st_size:
            raise ValueError('GLB declared length differs from file size')
        tail_start = stream.tell()
        original_tail_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
        changes = []
        for index, material in enumerate(document['materials']):
            if material.get('name') not in BISTRO_CUTOUT_MATERIALS:
                continue
            changes.append(dict(index=index, name=material['name'],
                                before=material.get('alphaMode', 'OPAQUE'), after='MASK'))
            material['alphaMode'] = 'MASK'
            material['alphaCutoff'] = .5
        if {entry['name'] for entry in changes} != BISTRO_CUTOUT_MATERIALS:
            raise ValueError('Expected both known foliage materials')
        encoded = json.dumps(document, separators=(',', ':')).encode()
        encoded += b' ' * (-len(encoded) % 4)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open('xb') as target:
            target.write(struct.pack('<III', 0x46546c67, 2, 20 + len(encoded) + size - tail_start))
            target.write(struct.pack('<II', len(encoded), 0x4e4f534a))
            target.write(encoded)
            stream.seek(tail_start)
            shutil.copyfileobj(stream, target)
    with output.open('rb') as stream:
        patched, declared = read_document(stream)
        copied_tail_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    with source.open('rb') as stream:
        original, _ = read_document(stream)
    for change in changes:
        patched['materials'][change['index']] = original['materials'][change['index']]
    if patched != original or original_tail_hash != copied_tail_hash or declared != output.stat().st_size:
        raise RuntimeError('Patch changed unrelated JSON, geometry or image bytes')
    geometry = {key: original.get(key) for key in ('accessors', 'bufferViews', 'buffers', 'meshes', 'nodes', 'scenes')}
    return dict(source=str(source.resolve()), output=str(output.resolve()),
                source_sha256=hash_file(source), output_sha256=hash_file(output), changes=changes,
                binary_chunks_sha256=original_tail_hash, binary_chunks_identical=True,
                geometry_metadata_sha256=hashlib.sha256(json.dumps(geometry, sort_keys=True).encode()).hexdigest(),
                unrelated_json_identical=True, bytes=output.stat().st_size)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = patch(args.source, args.output)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
