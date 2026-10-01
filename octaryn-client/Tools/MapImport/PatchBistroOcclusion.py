"""Omit Bistro's unsupported zero occlusion channel without re-exporting geometry.

Falcor documents packed R as unsupported:
https://github.com/NVIDIAGameWorks/Falcor/blob/master/docs/usage/scene-formats.md
Only matching source DDS + embedded zero-R images qualify. General glTF AO is
unchanged. Output must be a new artifact; BIN payload is verified unchanged.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import struct

from PIL import Image


def binary_digest(path):
    with path.open('rb') as source:
        source.seek(12)
        length, kind = struct.unpack('<II', source.read(8))
        assert kind == 0x4E4F534A
        source.seek(length, 1)
        return hashlib.file_digest(source, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--source-textures', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    original = args.input.resolve()
    output = args.output.resolve()
    if output == original or output.exists() or args.report.exists():
        raise ValueError('Output/report must be new paths; input cannot be overwritten')
    removed, retained = [], []
    with original.open('rb') as source:
        magic, version, size = struct.unpack('<III', source.read(12))
        if (magic, version) != (0x46546C67, 2) or size != original.stat().st_size:
            raise ValueError('Invalid GLB header')
        length, kind = struct.unpack('<II', source.read(8))
        if kind != 0x4E4F534A:
            raise ValueError('GLB JSON must be first')
        doc = json.loads(source.read(length))
        binary_header = source.tell()
        binary_length, binary_kind = struct.unpack('<II', source.read(8))
        if binary_kind != 0x004E4942:
            raise ValueError('Expected embedded BIN chunk')
        binary_start = source.tell()
        for material in doc.get('materials', []):
            info = material.get('occlusionTexture')
            if info is None:
                continue
            image = doc['images'][doc['textures'][info['index']]['source']]
            name = image.get('name', '')
            candidate = args.source_textures / (name + '.dds')
            if not name.endswith('_Specular') or not candidate.is_file():
                retained.append(material.get('name', ''))
                continue
            with Image.open(candidate) as raw:
                if raw.convert('RGB').getchannel('R').getextrema() != (0, 0):
                    raise ValueError(f'Authored nonzero source occlusion: {candidate.name}')
            view = doc['bufferViews'][image['bufferView']]
            offset = view.get('byteOffset', 0)
            if offset + view['byteLength'] > binary_length:
                raise ValueError('Image exceeds BIN bounds')
            source.seek(binary_start + offset)
            with Image.open(io.BytesIO(source.read(view['byteLength']))) as embedded:
                if embedded.convert('RGB').getchannel('R').getextrema() != (0, 0):
                    raise ValueError(f'Embedded AO differs from source: {name}')
            del material['occlusionTexture']
            removed.append(dict(material=material.get('name', ''), source=candidate.name))
        if not removed:
            raise ValueError('No matching unsupported Bistro occlusion found')
        encoded = json.dumps(doc, separators=(',', ':')).encode('utf-8')
        encoded += b' ' * (-len(encoded) % 4)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open('xb') as target:
            target.write(struct.pack('<III', magic, version, size - length + len(encoded)))
            target.write(struct.pack('<II', len(encoded), kind))
            target.write(encoded)
            source.seek(binary_header)
            shutil.copyfileobj(source, target, 1024 * 1024)
    before = binary_digest(original)
    after = binary_digest(output)
    if before != after:
        raise RuntimeError('BIN changed unexpectedly; do not stage this artifact')
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(dict(input=str(original), output=str(output),
        removed_count=len(removed), removed=removed, retained=retained,
        binary_sha256=before, binary_unchanged=True), indent=2))
    print(f'bistro_occlusion_patch=passed removed={len(removed)} retained={len(retained)} binary_unchanged=true')


if __name__ == '__main__':
    main()
