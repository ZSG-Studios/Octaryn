"""Run with Blender --background --python-exit-code 1 --python ... -- --output DIR."""
import argparse
import json
from pathlib import Path
import struct
import sys

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BistroMaterials import wire_bistro_material


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    textures = {}
    for suffix, values in (
        ('BaseColor', (.5, .3, .2, 1)), ('Specular', (.2, .6, .8, 1)),
        ('Normal_GL', (.5, .75, 1, 1)), ('Emissive', (.8, .4, .1, 1)),
    ):
        stem = 'Fixture_' + suffix
        image = bpy.data.images.new(stem, width=2, height=2)
        image.pixels[:] = values * 4
        image.filepath_raw = str(output / (stem + '.png'))
        image.file_format = 'PNG'
        image.save()
        textures[stem] = output / (stem + '.png')
    material = bpy.data.materials.new('Fixture')
    material.use_nodes = True
    bsdf = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    base = material.node_tree.nodes.new('ShaderNodeTexImage')
    base.image = bpy.data.images.load(str(textures['Fixture_BaseColor']))
    material.node_tree.links.new(base.outputs['Color'], bsdf.inputs['Base Color'])
    slots = wire_bistro_material(material, textures, 'Fixture_BaseColor')
    assert all(slots[name] for name in ('metallic_roughness', 'normal', 'emissive')), slots
    assert not slots['occlusion'], slots
    mesh = bpy.data.meshes.new('Fixture')
    mesh.from_pydata([(0, 0, 0), (1, 0, 0), (0, 1, 0)], [], [(0, 1, 2)])
    uv = mesh.uv_layers.new()
    for corner, coord in enumerate(((0, 0), (1, 0), (0, 1))):
        uv.data[corner].uv = coord
    obj = bpy.data.objects.new('Fixture', mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    target = output / 'bistro-material.glb'
    bpy.ops.export_scene.gltf(filepath=str(target), export_format='GLB', export_animations=False)
    blob = target.read_bytes()
    length, kind = struct.unpack_from('<II', blob, 12)
    assert kind == 0x4E4F534A
    doc = json.loads(blob[20:20 + length])
    exported = next(m for m in doc['materials'] if m['name'] == 'Fixture')
    pbr = exported['pbrMetallicRoughness']
    assert 'baseColorTexture' in pbr
    assert 'metallicRoughnessTexture' in pbr
    assert 'occlusionTexture' not in exported
    assert 'normalTexture' in exported
    assert 'emissiveTexture' in exported
    assert exported.get('emissiveFactor') == [1, 1, 1]
    print('bistro_material_export_checks=passed slots=4 unsupported_bistro_ao_omitted=true')


if __name__ == '__main__':
    main()
