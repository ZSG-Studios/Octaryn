"""Report conservative embedded GLB alpha recommendations without changing assets.

OPAQUE is exact: factor alpha one and every texel alpha 255. MASK is a
recommendation requiring both near-zero and near-one alpha, with at most 10%
of texels between 13 and 242. Review MASK recommendations against source
material intent: alpha histograms cannot prove that a material is foliage.
"""
import argparse
from collections import Counter
import io
import json
from pathlib import Path
import struct

from PIL import Image
from AnalyzeTextureAlpha import classify


def inspect(path):
    with path.open('rb') as source:
        magic, version, length = struct.unpack('<III', source.read(12))
        if magic != 0x46546c67 or version != 2 or length != path.stat().st_size:
            raise ValueError('Invalid GLB header')
        size, kind = struct.unpack('<II', source.read(8))
        if kind != 0x4e4f534a:
            raise ValueError('Missing GLB JSON chunk')
        doc = json.loads(source.read(size))
        binary_size, kind = struct.unpack('<II', source.read(8))
        if kind != 0x004e4942:
            raise ValueError('Missing GLB binary chunk')
        binary_start = source.tell()
        images = {}
        for index, entry in enumerate(doc.get('images', [])):
            if 'bufferView' not in entry:
                continue
            view = doc['bufferViews'][entry['bufferView']]
            offset, size = view.get('byteOffset', 0), view['byteLength']
            if view.get('buffer', 0) != 0 or offset + size > binary_size:
                raise ValueError('Image buffer outside GLB')
            source.seek(binary_start + offset)
            with Image.open(io.BytesIO(source.read(size))) as encoded:
                image = encoded.convert('RGBA')
                histogram = image.getchannel('A').histogram()
                images[index] = dict(recommendation=classify(image) or 'OPAQUE',
                                     minimum=image.getchannel('A').getextrema()[0],
                                     zero=histogram[0], one=histogram[255],
                                     intermediate=sum(histogram[1:255]), pixels=sum(histogram))
    materials = []
    for index, material in enumerate(doc.get('materials', [])):
        pbr = material.get('pbrMetallicRoughness', {})
        alpha = pbr.get('baseColorFactor', [1, 1, 1, 1])[3]
        original = material.get('alphaMode', 'OPAQUE')
        recommendation = original
        slot = pbr.get('baseColorTexture')
        image = None
        if slot:
            texture = doc['textures'][slot['index']]
            image = images.get(texture.get('source'))
        if original == 'BLEND' and alpha == 1 and image:
            recommendation = image['recommendation']
        materials.append(dict(index=index, name=material.get('name', ''), alpha_factor=alpha,
                              original=original, recommendation=recommendation,
                              image=image, requires_review=recommendation == 'MASK'))
    return dict(asset=str(path.resolve()), policy=__doc__,
                original_counts=dict(Counter(m['original'] for m in materials)),
                recommended_counts=dict(Counter(m['recommendation'] for m in materials)),
                materials=materials)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('glb', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    result = inspect(args.glb)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2))
    print(json.dumps({key: result[key] for key in ('original_counts', 'recommended_counts')}))


if __name__ == '__main__':
    main()
