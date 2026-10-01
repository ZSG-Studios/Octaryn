"""Headless Blender regression for opaque, factor-alpha and mask node wiring."""
from pathlib import Path
import sys

import bpy

sys.path.insert(0, str(Path(__file__).parent))
from ImportFbxMap import relink_material
from io_scene_gltf2.blender.exp.material.search_node_tree import detect_alpha_clip


def main():
    output = Path(__file__).resolve().parents[3] / 'build/release-windows/tools/map-alpha'
    output.mkdir(parents=True, exist_ok=True)
    image = bpy.data.images.new('alpha-fixture', width=1, height=1, alpha=True)
    image.pixels[:] = [1, 1, 1, 1]
    image.filepath_raw = str(output / 'alpha-fixture.png')
    image.file_format = 'PNG'
    image.save()
    for linked, factor, mode, expected in ((True, .3, 'OPAQUE', 1),
                                           (False, .3, 'OPAQUE', .3),
                                           (False, 1, 'MASK', None),
                                           (False, 1, 'BLEND', None)):
        material = bpy.data.materials.new('alpha-regression')
        material.use_nodes = True
        nodes = material.node_tree.nodes
        bsdf = next(node for node in nodes if node.type == 'BSDF_PRINCIPLED')
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = image
        material.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        alpha = bsdf.inputs['Alpha']
        alpha.default_value = factor
        if linked:
            material.node_tree.links.new(tex.outputs['Alpha'], alpha)
        relink_material(material, {'alpha-fixture': Path(image.filepath_raw)}, {'alpha-fixture': mode})
        if mode == 'OPAQUE':
            assert not alpha.is_linked and abs(alpha.default_value - expected) < .0001
        elif mode == 'MASK':
            assert alpha.links[0].from_node.operation == 'GREATER_THAN'
            assert alpha.links[0].from_node.inputs[1].default_value == .5
        else:
            assert alpha.links[0].from_node.type == 'TEX_IMAGE'
    assert callable(detect_alpha_clip)
    print('map_import_alpha=passed cases=4')


if __name__ == '__main__':
    main()
