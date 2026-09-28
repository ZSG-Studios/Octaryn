"""Bistro camera candidates tied to real material bounds, not visibility claims."""
import json
import math
import struct


VIEWS = {
    'foliage': dict(eye=[0, 2, -20], target=[-5, 4, -24],
                    materials=['Foliage_Leaves.DoubleSided', 'Foliage_Bux_Hedges46.DoubleSided']),
    'shop_glass': dict(eye=[0, 2, -12], target=[-1, 3, -5],
                      materials=['MASTER_Glass_Exterior']),
    'signs_metal': dict(eye=[-5.1, 6.0, 0.1], target=[-2.24, 5.99, -0.95],
                       materials=['Bistro_Sign_Main', 'Banner_Metal']),
    'chrome_interior': dict(eye=[5, 1.9, -9.8], target=[5.5996, 1.9125, -10.9533],
                           materials=['Metal_Chrome1']),
}


def views_for_map(path, names):
    # Read only the GLB JSON chunk; do not decode textures or geometry.
    with path.open('rb') as source:
        magic, version, _ = struct.unpack('<III', source.read(12))
        size, kind = struct.unpack('<II', source.read(8))
        if magic != 0x46546C67 or version != 2 or kind != 0x4E4F534A or size > 16 * 1024 * 1024:
            raise ValueError('Expected a bounded GLB 2 JSON chunk')
        asset = json.loads(source.read(size))
    if any(any(key in node for key in ('matrix', 'translation', 'rotation', 'scale'))
           for node in asset.get('nodes', [])):
        raise ValueError('Bistro view evidence requires baked world-coordinate geometry')
    materials = {material.get('name'): index for index, material in enumerate(asset['materials'])}
    result = {}
    for name in names:
        view = dict(VIEWS[name])
        evidence = []
        for material in view['materials']:
            if material not in materials:
                raise ValueError(f'Map lacks expected view material: {material}')
            bounds = []
            for mesh in asset['meshes']:
                for primitive in mesh['primitives']:
                    if primitive.get('material') == materials[material]:
                        positions = asset['accessors'][primitive['attributes']['POSITION']]
                        bounds.append([positions['min'], positions['max']])
            evidence.append(dict(material=material, bounds=bounds))
        delta = [b - a for a, b in zip(view['eye'], view['target'])]
        view['origin'] = view['eye'] + [math.atan2(delta[0], -delta[2]),
                                      math.atan2(delta[1], math.hypot(delta[0], delta[2]))]
        view['material_bounds'] = evidence
        view['coverage'] = 'candidate from material bounds; actual visibility requires image inspection'
        result[name] = view
    return result
