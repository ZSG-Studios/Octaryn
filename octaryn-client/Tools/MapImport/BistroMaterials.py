"""Bistro README texture conventions mapped to Blender's glTF exporter inputs."""
from pathlib import Path

import bpy


def _image(tree, path: Path, color_space: str):
    node = tree.nodes.new('ShaderNodeTexImage')
    node.image = bpy.data.images.load(str(path), check_existing=True)
    node.image.colorspace_settings.name = color_space
    return node


def wire_bistro_material(material, textures: dict[str, Path], base_stem: str) -> dict[str, bool]:
    """Wire only sibling maps documented by Bistro; preserve authored alpha."""
    slots = dict(metallic_roughness=False, occlusion=False, normal=False, emissive=False)
    if not base_stem.lower().endswith('_basecolor'):
        return slots
    prefix = base_stem[:-len('_BaseColor')]
    tree = material.node_tree
    bsdf = next((node for node in tree.nodes if node.type == 'BSDF_PRINCIPLED'), None)
    if bsdf is None:
        return slots
    lookup = {key.lower(): path for key, path in textures.items()}
    packed = lookup.get((prefix + '_Specular').lower())
    if packed:
        node = _image(tree, packed, 'Non-Color')
        separate = tree.nodes.new('ShaderNodeSeparateColor')
        separate.mode = 'RGB'
        tree.links.new(node.outputs['Color'], separate.inputs['Color'])
        # Falcor's scene-formats.md marks packed R occlusion unsupported.
        # All 201 Bistro source maps store zero there, not glTF visibility.
        tree.links.new(separate.outputs['Green'], bsdf.inputs['Roughness'])
        tree.links.new(separate.outputs['Blue'], bsdf.inputs['Metallic'])
        slots['metallic_roughness'] = True
    # The converter names inverted DirectX normals explicitly; never reuse a
    # source-convention image as an OpenGL/glTF tangent-space normal by accident.
    normal = lookup.get((prefix + '_Normal_GL').lower())
    if normal:
        node = _image(tree, normal, 'Non-Color')
        mapping = tree.nodes.new('ShaderNodeNormalMap')
        mapping.space = 'TANGENT'
        mapping.inputs['Strength'].default_value = 1.0
        tree.links.new(node.outputs['Color'], mapping.inputs['Color'])
        tree.links.new(mapping.outputs['Normal'], bsdf.inputs['Normal'])
        slots['normal'] = True
    emissive = lookup.get((prefix + '_Emissive').lower())
    if emissive:
        node = _image(tree, emissive, 'sRGB')
        tree.links.new(node.outputs['Color'], bsdf.inputs['Emission Color'])
        bsdf.inputs['Emission Strength'].default_value = 1.0
        slots['emissive'] = True
    return slots
