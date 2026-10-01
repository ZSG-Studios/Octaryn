"""Exact spatial/material triangle batching with preserved corner attributes."""
import math

import bpy
import numpy as np


def array(collection, field, width, dtype=np.float32):
    result = np.empty(len(collection) * width, dtype=dtype)
    collection.foreach_get(field, result)
    return result.reshape((-1, width))


def triangle_data(obj):
    mesh = obj.data
    mesh.calc_loop_triangles()
    vertices = array(mesh.vertices, 'co', 3)
    corners = array(mesh.loop_triangles, 'loops', 3, np.int32)
    vertex_ids = array(mesh.loops, 'vertex_index', 1, np.int32).reshape(-1)
    positions = vertices[vertex_ids[corners]]
    materials = array(mesh.loop_triangles, 'material_index', 1, np.int32).reshape(-1)
    return corners, positions, materials, vertex_ids


def keys_for(obj, positions, materials, cell_size):
    cells = np.floor(positions.mean(axis=1) / cell_size).astype(np.int32)
    keys = np.column_stack((cells, materials))
    unique, inverse = np.unique(keys, axis=0, return_inverse=True)
    for number, key in enumerate(unique):
        slot = int(key[3])
        material = obj.data.materials[slot] if slot < len(obj.data.materials) else None
        yield (tuple(int(x) for x in key[:3]), material.name if material else ''), material, inverse == number


def partition_meshes(meshes, cell_size=16.0, max_primitives=4096):
    if not math.isfinite(cell_size) or cell_size <= 0 or max_primitives < 1:
        raise ValueError('Invalid spatial partition limits')
    # Imported transforms must be baked before spatial keys and normals are read.
    for obj in meshes:
        if any(abs(obj.matrix_world[r][c] - float(r == c)) > 1e-5 for r in range(4) for c in range(4)):
            raise ValueError(f'Unbaked map transform: {obj.name}')
    for obj in meshes:
        obj.data.calc_loop_triangles()
    original_count = sum(len(obj.data.loop_triangles) for obj in meshes)
    requested_size = cell_size
    while True:
        groups = set()
        for obj in meshes:
            _, positions, materials, _ = triangle_data(obj)
            groups.update(key for key, _, _ in keys_for(obj, positions, materials, cell_size))
        if len(groups) <= max_primitives:
            break
        if cell_size > 1e6:
            raise ValueError('Material count exceeds map primitive limit')
        cell_size *= 2
    batches = {}
    for obj in meshes:
        mesh = obj.data
        corners, positions, materials, vertex_ids = triangle_data(obj)
        normals = array(mesh.corner_normals, 'vector', 3)
        uv = {layer.name: array(layer.data, 'uv', 2) for layer in mesh.uv_layers}
        colors = {}
        for attribute in mesh.color_attributes:
            values = array(attribute.data, 'color', 4)
            colors[attribute.name] = values[vertex_ids] if attribute.domain == 'POINT' else values
        for key, material, mask in keys_for(obj, positions, materials, cell_size):
            loops = corners[mask].reshape(-1)
            active_uv = next((layer.name for layer in mesh.uv_layers if layer.active_render), None)
            active_color = mesh.color_attributes.active_color_name
            batch = batches.setdefault(key, {'material': material, 'chunks': [],
                                             'active_uv': active_uv, 'active_color': active_color})
            if batch['active_uv'] != active_uv or batch['active_color'] != active_color:
                raise ValueError(f'Inconsistent active UV/color layers within material batch: {key}')
            batch['chunks'].append((positions[mask].reshape((-1, 3)), normals[loops],
                                    {name: values[loops] for name, values in uv.items()},
                                    {name: values[loops] for name, values in colors.items()}))
        bpy.data.objects.remove(obj, do_unlink=True)
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    result = []
    triangle_count = 0
    for number, key in enumerate(sorted(batches)):
        batch = batches.pop(key)
        chunks = batch['chunks']
        positions = np.concatenate([chunk[0] for chunk in chunks])
        normals = np.concatenate([chunk[1] for chunk in chunks])
        unique, indices = np.unique(positions, axis=0, return_inverse=True)
        mesh = bpy.data.meshes.new(f'MapCell{number:04d}')
        mesh.vertices.add(len(unique))
        mesh.vertices.foreach_set('co', unique.reshape(-1))
        mesh.loops.add(len(indices))
        mesh.loops.foreach_set('vertex_index', indices.astype(np.int32))
        mesh.polygons.add(len(indices) // 3)
        mesh.polygons.foreach_set('loop_start', np.arange(0, len(indices), 3, dtype=np.int32))
        mesh.polygons.foreach_set('loop_total', np.full(len(indices) // 3, 3, dtype=np.int32))
        mesh.polygons.foreach_set('use_smooth', np.ones(len(indices) // 3, dtype=bool))
        mesh.update()
        mesh.normals_split_custom_set(normals.tolist())
        for slot, width in ((2, 2), (3, 4)):
            for name in dict.fromkeys(name for chunk in chunks for name in chunk[slot]):
                default = 0 if slot == 2 else 1
                values = np.concatenate([chunk[slot].get(name, np.full((len(chunk[0]), width), default, np.float32))
                                         for chunk in chunks])
                if slot == 2:
                    mesh.uv_layers.new(name=name).data.foreach_set('uv', values.reshape(-1))
                else:
                    mesh.color_attributes.new(name=name, type='FLOAT_COLOR', domain='CORNER').data.foreach_set('color', values.reshape(-1))
        if batch['active_uv']:
            mesh.uv_layers[batch['active_uv']].active_render = True
            mesh.uv_layers.active = mesh.uv_layers[batch['active_uv']]
        if batch['active_color']:
            mesh.color_attributes.active_color_name = batch['active_color']
        if batch['material']:
            mesh.materials.append(batch['material'])
        obj = bpy.data.objects.new(mesh.name, mesh)
        bpy.context.collection.objects.link(obj)
        result.append(obj)
        triangle_count += len(mesh.polygons)
    if triangle_count != original_count:
        raise AssertionError(f'Spatial partition changed triangles: {original_count} -> {triangle_count}')
    return result, {'requested_cell_size': requested_size, 'cell_size': cell_size,
                    'primitives': len(result), 'triangles_before': original_count,
                    'triangles_after': triangle_count, 'simplification': False}
