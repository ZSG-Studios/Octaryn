"""Small CPU regression: shared mirrored-UV seams, exact attributes, spatial cells."""
import argparse
import json
from pathlib import Path
import struct
import tempfile

import numpy as np

from CookGlb import cook
from GlbCookIO import Glb
from ValidateGlbCook import validate
from GlbMikk import Mikk


def fixture(path, blend=False):
    binary = bytearray()
    doc = {'asset': {'version': '2.0'}, 'scene': 0, 'scenes': [{'nodes': [0]}],
           'nodes': [{'mesh': 0}], 'meshes': [], 'accessors': [], 'bufferViews': [],
           'materials': [{'name': 'Preserved', 'pbrMetallicRoughness': {'roughnessFactor': .7}}]}
    if blend:
        doc['materials'][0]['alphaMode'] = 'BLEND'

    def put(values, kind, component):
        binary.extend(b'\0' * (-len(binary) % 4))
        raw = values.tobytes()
        doc['bufferViews'].append({'buffer': 0, 'byteOffset': len(binary), 'byteLength': len(raw)})
        binary.extend(raw)
        doc['accessors'].append({'bufferView': len(doc['bufferViews']) - 1, 'componentType': component,
                                 'type': kind, 'count': len(values)})
        return len(doc['accessors']) - 1
    positions = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0],
                          [32, 0, 0], [33, 0, 0], [32, 1, 0]], '<f4')
    attributes = {'POSITION': put(positions, 'VEC3', 5126),
                  'NORMAL': put(np.tile(np.array([0, 0, 1], '<f4'), (7, 1)), 'VEC3', 5126),
                  'TEXCOORD_0': put(np.array([[0, 0], [1, 0], [0, 1], [1, 0], [0, 0], [1, 0], [0, 1]], '<f4'), 'VEC2', 5126),
                  'TEXCOORD_1': put(np.full((7, 2), .25, '<f4'), 'VEC2', 5126),
                  'COLOR_0': put(np.full((7, 4), 127, 'u1'), 'VEC4', 5121)}
    doc['accessors'][attributes['COLOR_0']]['normalized'] = True
    indices = put(np.array([0, 1, 2, 4, 5, 6, 0, 2, 3], '<u4'), 'SCALAR', 5125)
    doc['meshes'] = [{'primitives': [{'attributes': attributes, 'indices': indices, 'material': 0}]}]
    doc['buffers'] = [{'byteLength': len(binary)}]
    encoded = json.dumps(doc).encode()
    encoded += b' ' * (-len(encoded) % 4)
    path.write_bytes(struct.pack('<5I', 0x46546c67, 2, 28 + len(encoded) + len(binary), len(encoded), 0x4e4f534a)
                     + encoded + struct.pack('<II', len(binary), 0x004e4942) + binary)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--library', type=Path, required=True)
    args = parser.parse_args()
    # Projected-angle weighting rounds to zero with almost in-plane normals.
    mikk = Mikk(args.library)
    positions = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0]], '<f4')
    normal = np.array([1, 2, .0001], '<f4'); normal /= np.linalg.norm(normal)
    normals = np.tile(normal, (3, 1))
    uv = np.array([[0, 0], [1, 0], [0, 1]], '<f4')
    indices = np.arange(3, dtype='<u4')
    regular = mikk.generate(positions, normals, uv, indices)
    mirrored = mikk.generate(positions, normals, uv * np.array([-1, 1], '<f4'), indices)
    assert mikk.zero_angle_corners > 0, 'Synthetic corner did not exercise zero-angle repair'
    assert np.allclose(regular[:, 3], -mirrored[:, 3])
    assert np.all(np.abs(np.sum(regular[:, :3] * normals, axis=1)) < .002)
    flat_normals = np.tile(np.array([1, 0, 0], '<f4'), (3, 1))
    collapsed = mikk.generate(positions, flat_normals, uv, indices)
    assert mikk.collapsed_derivative_corners == 3
    assert np.all(np.abs(np.sum(collapsed[:, :3] * flat_normals, axis=1)) < .002)
    degenerate = mikk.generate(positions, flat_normals, np.zeros((3, 2), '<f4'), indices)
    assert mikk.degenerate_corners == 3
    assert np.allclose(np.linalg.norm(degenerate[:, :3], axis=1), 1)
    assert np.all(np.abs(np.sum(degenerate[:, :3] * flat_normals, axis=1)) < .002)
    with tempfile.TemporaryDirectory() as directory:
        source, output = Path(directory) / 'source.glb', Path(directory) / 'candidate.glb'
        fixture(source)
        report = cook(source, output, args.library)
        result = validate(source, output, report)
        assert report['triangles'] == 3 and report['primitives'] == 2
        assert report['vertices'] == 9, 'Mirrored seam must split original shared vertices'
        candidate = Glb(output)
        signs = set()
        for mesh in candidate.doc['meshes']:
            for primitive in mesh['primitives']:
                signs.update(candidate.accessor(primitive['attributes']['TANGENT'])[:, 3])
        assert signs == {-1, 1}
        candidate.data.close(); candidate.file.close()
        blend_source, blend_output = Path(directory) / 'blend.glb', Path(directory) / 'blend_candidate.glb'
        fixture(blend_source, blend=True)
        blend_report = cook(blend_source, blend_output, args.library)
        assert blend_report['primitives'] == 1
        validate(blend_source, blend_output, blend_report)
        try:
            cook(source, output, args.library)
        except ValueError:
            pass
        else:
            raise AssertionError('Existing output was overwritten')
        print('bounded_glb_cook_tests=passed ' + json.dumps(result))


if __name__ == '__main__':
    main()
