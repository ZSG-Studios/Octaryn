"""Bounded exact GLB spatial/tangent cook; preserves encoded images and materials."""
import argparse
import copy
import json
import os
from pathlib import Path
import time

os.environ['OMP_NUM_THREADS'] = '1'
os.environ['OPENBLAS_NUM_THREADS'] = '1'
import numpy as np

from GlbCookBudget import start_guard
from GlbCookIO import Glb, Writer
from GlbMikk import Mikk, split_vertices
from ZeroBasis import validate as validate_zero_basis


def primitives(source):
    for mesh_index, mesh in enumerate(source.doc['meshes']):
        for primitive_index, primitive in enumerate(mesh['primitives']):
            yield mesh_index, primitive_index, primitive


def geometry(source, primitive):
    if primitive.get('mode', 4) != 4 or primitive.get('targets') or primitive.get('extensions'):
        raise ValueError('Cook requires static uncompressed triangle lists')
    indices = source.accessor(primitive['indices']).reshape(-1)
    attributes = {name: source.accessor(index) for name, index in primitive['attributes'].items()}
    count = len(attributes['POSITION'])
    if len(indices) % 3 or indices.max() >= count or any(len(a) != count for a in attributes.values()):
        raise ValueError('Invalid primitive attribute/index counts')
    # Conservative estimate covers native Mikk scratch and sorting/copy buffers.
    if len(indices) * 400 > 1024**3:
        raise ValueError('Primitive exceeds 1 GiB estimated working budget')
    return indices, attributes


def cells(positions, indices, size):
    centers = positions[indices.reshape((-1, 3))].mean(axis=1)
    if not np.all(np.isfinite(centers)):
        raise ValueError('Nonfinite map positions')
    return np.unique(np.floor(centers / size).astype(np.int32), axis=0, return_inverse=True)


def float_values(source, primitive, name, value):
    a = source.doc['accessors'][primitive['attributes'][name]]
    result = np.asarray(value, dtype=np.float32)
    if a.get('normalized') and value.dtype.kind != 'f':
        maximum = np.iinfo(value.dtype).max
        result = np.maximum(result / maximum, -1)
    return result


def primitive_cells(source, primitive, positions, indices, size):
    material = source.doc.get('materials', [])[primitive['material']] if 'material' in primitive else {}
    if material.get('alphaMode') == 'BLEND':
        # Keep authored transparent triangle ordering and the existing sort unit.
        return np.zeros((1, 3), dtype=np.int32), np.zeros(len(indices) // 3, dtype=np.int32)
    return cells(positions, indices, size)


def cook(source_path, output, library, cell_size=16):
    source = Glb(source_path)
    if source.doc.get('skins') or source.doc.get('animations'):
        raise ValueError('Static maps only')
    if any(any(k in node for k in ('matrix', 'translation', 'rotation', 'scale', 'skin', 'weights'))
           for node in source.doc['nodes']):
        raise ValueError('Cook requires baked identity node transforms')
    if cell_size <= 0 or not np.isfinite(cell_size):
        raise ValueError('Invalid cell size')
    while True:
        count = 0
        for _, _, primitive in primitives(source):
            indices, attributes = geometry(source, primitive)
            keys, _ = primitive_cells(source, primitive, attributes['POSITION'], indices, cell_size)
            count += len(keys)
        if count <= 4096:
            break
        cell_size *= 2
        if cell_size > 1e6:
            raise ValueError('Too many source primitives')
    writer = Writer(source, output)
    for mesh in writer.doc['meshes']:
        mesh['primitives'] = []
    mikk = Mikk(library)
    report = {'source': str(source_path), 'output': str(output), 'cell_size': cell_size,
              'mikk_pin': '3e895b49d05ea07e4c2133156cfa94369e19e409', 'groups': [],
              'triangles': 0, 'vertices': 0, 'primitives': count}
    for mesh_index, primitive_index, primitive in primitives(source):
        indices, attributes = geometry(source, primitive)
        material = source.doc.get('materials', [])[primitive['material']] if 'material' in primitive else {}
        normal = material.get('normalTexture', {})
        uv_name = f'TEXCOORD_{normal.get("texCoord", 0)}'
        if normal.get('extensions'):
            raise ValueError('Normal UV transforms need a separately qualified tangent cook')
        if 'NORMAL' not in attributes or uv_name not in attributes:
            raise ValueError('Authored tangents require source NORMAL and selected UV')
        zero_basis = validate_zero_basis(source, primitive, attributes, indices)
        tangent = None if zero_basis else mikk.generate(attributes['POSITION'], float_values(source, primitive, 'NORMAL', attributes['NORMAL']),
                                                       float_values(source, primitive, uv_name, attributes[uv_name]), indices)
        keys, grouping = primitive_cells(source, primitive, attributes['POSITION'], indices, cell_size)
        outputs = []
        for number in range(len(keys)):
            selected = np.flatnonzero(grouping == number)
            corner_ids = (selected[:, None] * 3 + np.arange(3)).reshape(-1)
            if zero_basis:
                original_ids, remapped = np.unique(indices[corner_ids], return_inverse=True)
                remapped = remapped.astype(np.uint32)
            else:
                original_ids, frames, remapped = split_vertices(indices[corner_ids], tangent[corner_ids])
            new = copy.deepcopy(primitive)
            new['attributes'] = {}
            for name, values in attributes.items():
                if name == 'TANGENT':
                    continue
                new['attributes'][name] = writer.accessor(values[original_ids],
                    source.doc['accessors'][primitive['attributes'][name]], name == 'POSITION')
            if not zero_basis:
                new['attributes']['TANGENT'] = writer.accessor(frames)
            new['indices'] = writer.accessor(remapped)
            outputs.append(len(writer.doc['meshes'][mesh_index]['primitives']))
            writer.doc['meshes'][mesh_index]['primitives'].append(new)
            report['vertices'] += len(original_ids)
        report['groups'].append({'mesh': mesh_index, 'source_primitive': primitive_index, 'outputs': outputs})
        report['triangles'] += len(indices) // 3
        print(f'cook_primitive={len(report["groups"])} triangles={len(indices)//3} cells={len(keys)}', flush=True)
    writer.finish()
    report['degenerate_tangent_corners'] = mikk.degenerate_corners
    report['singular_smoothed_tangent_corners'] = mikk.singular_smoothed_corners
    report['zero_angle_tangent_corners'] = mikk.zero_angle_corners
    report['collapsed_derivative_tangent_corners'] = mikk.collapsed_derivative_corners
    report['orthogonalized_tangent_corners'] = mikk.orthogonalized_corners
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--cell-size', type=float, default=16)
    args = parser.parse_args()
    if args.output.resolve() == args.input.resolve() or args.report.exists():
        raise ValueError('Cook never overwrites input or existing report')
    state, stop = start_guard()
    started = time.monotonic()
    report = cook(args.input, args.output, args.library, args.cell_size)
    report.update(state, elapsed_seconds=time.monotonic() - started)
    args.report.write_text(json.dumps(report, indent=2))
    stop.set()
    print(f'cook_complete=true triangles={report["triangles"]} primitives={report["primitives"]} '
          f'peak_private_mib={report["peak_private_bytes"]/2**20:.1f}', flush=True)


if __name__ == '__main__':
    main()
