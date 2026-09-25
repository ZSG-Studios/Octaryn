"""Headless Blender exact batching and mirrored-UV tangent export regression."""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import sys

import bpy
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ImportFbxMap import export_glb
from MapMeshPartition import partition_meshes, triangle_data


def signature(objects):
    triangles = Counter()
    for obj in objects:
        corners, positions, _, _ = triangle_data(obj)
        for loops, points in zip(corners, positions):
            triangles[tuple((tuple(float(x) for x in point),
                             tuple((layer.name, tuple(round(x, 5) for x in layer.data[int(loop)].uv))
                                   for layer in obj.data.uv_layers),
                             tuple((layer.name, tuple(round(x, 5) for x in layer.data[int(loop)].color))
                                   for layer in obj.data.color_attributes))
                            for point, loop in zip(points, loops))] += 1
    return triangles


def normal_vectors(objects):
    result = {}
    for obj in objects:
        corners, positions, _, _ = triangle_data(obj)
        for loops, points in zip(corners, positions):
            result[tuple(points.reshape(-1))] = np.array([obj.data.corner_normals[int(i)].vector[:] for i in loops])
    return result


def accessor(doc, binary, index):
    a = doc['accessors'][index]
    view = doc['bufferViews'][a['bufferView']]
    widths = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}
    formats = {5126: 'f', 5125: 'I', 5123: 'H', 5121: 'B'}
    fmt = '<' + formats[a['componentType']] * widths[a['type']]
    stride = view.get('byteStride', struct.calcsize(fmt))
    offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
    return [struct.unpack_from(fmt, binary, offset + n * stride) for n in range(a['count'])]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    material = bpy.data.materials.new('NormalMapped')
    material.use_nodes = True
    tree = material.node_tree
    image = bpy.data.images.new('FixtureNormal', width=1, height=1)
    image.pixels[:] = [.5, .5, 1, 1]
    texture = tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    normal = tree.nodes.new('ShaderNodeNormalMap')
    tree.links.new(texture.outputs['Color'], normal.inputs['Color'])
    tree.links.new(normal.outputs['Normal'], tree.nodes.get('Principled BSDF').inputs['Normal'])
    mesh = bpy.data.meshes.new('SeparatedQuads')
    mesh.from_pydata([(x + offset, y, 0) for offset in (0, 32) for x, y in ((0, 0), (1, 0), (1, 1), (0, 1))],
                     [], [(0, 1, 2, 3), (4, 5, 6, 7)])
    uv = mesh.uv_layers.new(name='UVMap')
    for loop in mesh.loops:
        vertex = mesh.vertices[loop.vertex_index].co
        uv.data[loop.index].uv = (vertex.x if loop.vertex_index < 4 else 33 - vertex.x, vertex.y)
    uv1 = mesh.uv_layers.new(name='UVDetail')
    colors = mesh.color_attributes.new(name='Color', type='FLOAT_COLOR', domain='CORNER')
    for loop in mesh.loops:
        uv1.data[loop.index].uv = (.1 * loop.index, .3)
        colors.data[loop.index].color = (.1 * loop.index, .2, .3, 1)
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    mesh.normals_split_custom_set([(0, .3, .9539392)] * len(mesh.loops))
    mesh.materials.append(material)
    obj = bpy.data.objects.new('Fixture', mesh)
    bpy.context.collection.objects.link(obj)
    before = signature([obj])
    before_normals = normal_vectors([obj])
    objects, report = partition_meshes([obj], 16)
    assert report['primitives'] == 2 and report['triangles_after'] == 4, report
    after = signature(objects)
    assert after == before, f'Corner attributes changed: {list((before - after).items())[:1]} -> {list((after - before).items())[:1]}'
    for triangle, normals in normal_vectors(objects).items():
        assert np.allclose(normals, before_normals[triangle], atol=2e-4), 'Custom split normal changed'
    args.output.mkdir(parents=True, exist_ok=True)
    path = args.output / 'partition.glb'
    export_glb(path)
    data = path.read_bytes()
    size = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20 + size])
    binary = data[28 + size:]
    handedness = set()
    triangles = 0
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            attributes = primitive['attributes']
            assert 'TANGENT' in attributes
            tangents = np.array(accessor(doc, binary, attributes['TANGENT']))
            normals = np.array(accessor(doc, binary, attributes['NORMAL']))
            assert np.all(np.abs(np.linalg.norm(tangents[:, :3], axis=1) - 1) < 1e-5)
            assert np.all(np.abs(np.sum(tangents[:, :3] * normals, axis=1)) < 1e-5)
            handedness.update(tangents[:, 3])
            triangles += len(accessor(doc, binary, primitive['indices'])) // 3
    assert triangles == 4 and handedness == {-1, 1}, (triangles, handedness)
    objects, bounded = partition_meshes(objects, 16, max_primitives=1)
    assert bounded['primitives'] == 1 and bounded['cell_size'] == 64, bounded
    assert signature(objects) == before, 'Bounded coarsening changed geometry/attributes'
    print('map_partition_checks=passed exact_triangles=4 spatial_batches=2 mirrored_uv_tangents=true', flush=True)


if __name__ == '__main__':
    main()
