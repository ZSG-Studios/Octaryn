"""Validate source BC5 decoding and material-channel roundtrips without Blender."""
import argparse
import io
import json
import math
from pathlib import Path
import struct
import tempfile

from PIL import Image
from ConvertMapTextures import convert_one, normal_rgb, resize_normal


def check_unit_normals(image):
    assert image.getchannel('B').getextrema()[0] >= 127
    pixels = iter(image.tobytes())
    for rgb in zip(pixels, pixels, pixels):
        length = math.sqrt(sum((c * (2.0 / 255.0) - 1) ** 2 for c in rgb))
        assert abs(length - 1) < .014, (rgb, length)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--normal-source', required=True, type=Path)
    parser.add_argument('--material-fixture', required=True, type=Path)
    args = parser.parse_args()
    source = Image.open(args.normal_source).convert('RGB')
    decoded = normal_rgb(args.normal_source, source)
    for x, y in ((0, 0), (source.width // 2, source.height // 2), (source.width - 1, source.height - 1)):
        r, g, _ = source.getpixel((x, y))
        z = math.sqrt(max(0, 1 - (r / 255 * 2 - 1) ** 2 - (g / 255 * 2 - 1) ** 2))
        actual = decoded.getpixel((x, y))
        assert actual[0] == r and actual[1] == 255 - g, actual
        assert abs(actual[2] - round((z * .5 + .5) * 255)) <= 1, actual
    # Exercise production resizing of an actual BC5 DDS without replacing assets.
    with tempfile.TemporaryDirectory() as directory:
        convert_one(args.normal_source, Path(directory), 127)
        resized = Image.open(Path(directory) / (args.normal_source.stem + '_GL.png')).convert('RGB')
        assert max(resized.size) <= 127
        check_unit_normals(resized)
    edge = Image.new('RGB', (16, 16))
    edge.putdata([(255, 128, 128) if x < 8 else (0, 128, 255)
                  for y in range(16) for x in range(16)])
    check_unit_normals(resize_normal(edge, (7, 7)))
    # Opposed tangential directions must average to a stable positive-Z normal.
    opposed = Image.new('RGB', (2, 1))
    opposed.putdata([(255, 255, 128), (0, 0, 128)])
    check_unit_normals(resize_normal(opposed, (1, 1)))
    root = args.material_fixture
    blob = (root / 'bistro-material.glb').read_bytes()
    length = struct.unpack_from('<I', blob, 12)[0]
    doc = json.loads(blob[20:20 + length])
    material = next(m for m in doc['materials'] if m['name'] == 'Fixture')
    texture = material['pbrMetallicRoughness']['metallicRoughnessTexture']['index']
    image = doc['images'][doc['textures'][texture]['source']]
    view = doc['bufferViews'][image['bufferView']]
    start = 28 + length + view.get('byteOffset', 0)
    embedded = Image.open(io.BytesIO(blob[start:start + view['byteLength']])).convert('RGB')
    packed = Image.open(root / 'Fixture_Specular.png').convert('RGB')
    assert embedded.size == packed.size and embedded.tobytes() == packed.tobytes()
    print('bistro_texture_checks=passed bc5_z=true directx_y_inverted=true '
          'resized_positive_z_unit_normals=true packed_orm_exact=true')


if __name__ == '__main__':
    main()
