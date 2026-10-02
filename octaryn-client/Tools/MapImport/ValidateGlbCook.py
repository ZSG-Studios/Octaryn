"""Independent candidate parity checks using exact per-triangle attribute bytes."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from GlbCookIO import Glb
from ZeroBasis import declared, validate as validate_zero_basis


def triangle_records(source, primitive, names):
    ids = source.accessor(primitive['indices']).reshape(-1)
    arrays = []
    for name in names:
        values = np.ascontiguousarray(source.accessor(primitive['attributes'][name])[ids])
        arrays.append(values.view(np.uint8).reshape((len(ids) // 3, -1)))
    return np.concatenate(arrays, axis=1)


def digest(records):
    rows = np.ascontiguousarray(records).view(np.dtype((np.void, records.shape[1]))).reshape(-1)
    return hashlib.sha256(np.sort(rows).tobytes()).hexdigest()


def validate(source_path, output_path, report):
    source, output = Glb(source_path), Glb(output_path)
    for key in ('materials', 'textures', 'samplers', 'nodes', 'scenes', 'scene', 'extensionsUsed', 'extensionsRequired'):
        if source.doc.get(key) != output.doc.get(key):
            raise AssertionError(f'Metadata changed: {key}')
    if source.image_hashes() != output.image_hashes():
        raise AssertionError('Encoded image bytes changed')
    for a, b in zip(source.doc.get('images', []), output.doc.get('images', [])):
        if {k: v for k, v in a.items() if k != 'bufferView'} != {k: v for k, v in b.items() if k != 'bufferView'}:
            raise AssertionError('Image metadata changed')
    expected_groups = {(m, p) for m, mesh in enumerate(source.doc['meshes']) for p in range(len(mesh['primitives']))}
    source_groups = [(g['mesh'], g['source_primitive']) for g in report['groups']]
    output_groups = [(g['mesh'], p) for g in report['groups'] for p in g['outputs']]
    expected_output = {(m, p) for m, mesh in enumerate(output.doc['meshes']) for p in range(len(mesh['primitives']))}
    if set(source_groups) != expected_groups or len(source_groups) != len(expected_groups):
        raise AssertionError('Source primitives omitted/duplicated')
    if set(output_groups) != expected_output or len(output_groups) != len(expected_output) or len(expected_output) > 4096:
        raise AssertionError('Candidate primitives omitted/duplicated or exceed limit')
    triangles, frames, max_dot = 0, 0, 0
    for group in report['groups']:
        primitive = source.doc['meshes'][group['mesh']]['primitives'][group['source_primitive']]
        material = source.doc.get('materials', [])[primitive['material']] if 'material' in primitive else {}
        zero_basis = declared(material)
        names = sorted(name for name in primitive['attributes'] if name != 'TANGENT')
        expected = triangle_records(source, primitive, names)
        actual = []
        for index in group['outputs']:
            candidate = output.doc['meshes'][group['mesh']]['primitives'][index]
            if candidate.get('material') != primitive.get('material') or candidate.get('mode', 4) != 4:
                raise AssertionError('Material/topology changed')
            if sorted(candidate['attributes']) != sorted(names if zero_basis else names + ['TANGENT']):
                raise AssertionError('Vertex attributes changed')
            actual.append(triangle_records(output, candidate, names))
            if zero_basis:
                validate_zero_basis(output, candidate, {name: output.accessor(index) for name, index in candidate['attributes'].items()}, output.accessor(candidate['indices']).reshape(-1))
                continue
            tangent = output.accessor(candidate['attributes']['TANGENT'])
            normals = output.accessor(candidate['attributes']['NORMAL']).astype(np.float32)
            normals /= np.linalg.norm(normals, axis=1)[:, None]
            if not np.all(np.isfinite(tangent)) or np.any(np.abs(np.linalg.norm(tangent[:, :3], axis=1) - 1) > .002):
                raise AssertionError('Nonunit/nonfinite tangent')
            if np.any(np.abs(tangent[:, 3]) != 1):
                raise AssertionError('Invalid tangent handedness')
            dot = float(np.max(np.abs(np.sum(normals * tangent[:, :3], axis=1))))
            if dot > .002:
                raise AssertionError(f'Nonorthogonal tangent: {dot} mesh={group["mesh"]} '
                                     f'source_primitive={group["source_primitive"]} output_primitive={index}')
            max_dot = max(max_dot, dot)
            frames += len(tangent)
        actual = np.concatenate(actual)
        if actual.shape != expected.shape or digest(actual) != digest(expected):
            raise AssertionError('Triangle winding/position/normal/UV/color bytes changed')
        material = source.doc.get('materials', [])[primitive['material']] if 'material' in primitive else {}
        if material.get('alphaMode') == 'BLEND' and (len(group['outputs']) != 1 or not np.array_equal(actual, expected)):
            raise AssertionError('Transparent triangle ordering or sort unit changed')
        triangles += len(expected)
    if triangles != report['triangles']:
        raise AssertionError('Triangle report mismatch')
    return {'triangle_attribute_parity': True, 'image_bytes_unchanged': True, 'materials_unchanged': True,
            'triangles': triangles, 'tangent_frames': frames, 'maximum_tangent_normal_dot': max_dot}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--validation', type=Path, required=True)
    args = parser.parse_args()
    from GlbCookBudget import start_guard
    state, stop = start_guard()
    result = validate(args.input, args.output, json.loads(args.report.read_text()))
    result.update(state)
    with args.validation.open('x') as stream:
        json.dump(result, stream, indent=2)
    stop.set()
    print('glb_cook_parity=passed ' + json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
