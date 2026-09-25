"""Small deterministic glTF fixtures exercising production map decoding."""
import json
import math
import struct


def fixture(path, mode=4, normals=True, mirrored=False, colors=4, invalid=False,
            texture_uv=None, pbr=False, morph=False):
    positions = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0)]
    indices = [0, 1, 2, 2, 1, 3] if mode == 4 else [0, 1, 2, 3]
    if mode == 6:
        positions = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]
    if invalid:
        indices[-1] = 20
    blob = bytearray()
    views, accessors = [], []

    def accessor(values, kind, component=5126):
        while len(blob) % 4:
            blob.append(0)
        flat = [v for row in values for v in row] if kind != 'SCALAR' else values
        data = struct.pack('<' + ('f' if component == 5126 else 'I') * len(flat), *flat)
        views.append(dict(buffer=0, byteOffset=len(blob), byteLength=len(data)))
        blob.extend(data)
        accessors.append(dict(bufferView=len(views) - 1, componentType=component,
                              count=len(values), type=kind))
        return len(accessors) - 1

    attrs = {'POSITION': accessor(positions, 'VEC3')}
    accessors[0].update(min=[0, 0, 0], max=[1, 1, 0])
    if normals:
        attrs['NORMAL'] = accessor([(0, 0, 1)] * 4, 'VEC3')
    attrs['TEXCOORD_1'] = accessor([(.25, .75)] * 4, 'VEC2')
    attrs['TANGENT'] = accessor([(1, 0, 0, 1)] * 4, 'VEC4')
    attrs['COLOR_0'] = accessor([(.2, .4, .6, .8)[:colors]] * 4, f'VEC{colors}')
    primitive = dict(attributes=attrs, indices=accessor(indices, 'SCALAR', 5125), mode=mode)
    if morph:
        target = accessor([(0, 0, 1)] * 4, 'VEC3')
        accessors[target].update(min=[0, 0, 1], max=[0, 0, 1])
        primitive['targets'] = [{'POSITION': target}]
    node = dict(mesh=0, translation=[2, 3, 4], scale=[-2 if mirrored else 2, 3, 1])
    doc = dict(asset={'version': '2.0'}, scene=0, scenes=[{'nodes': [0]}],
               nodes=[node], meshes=[{'primitives': [primitive]}],
               buffers=[{'byteLength': len(blob)}], bufferViews=views, accessors=accessors)
    if morph:
        doc['meshes'][0]['weights'] = [1]
    if texture_uv is not None:
        # Loader fixtures inspect image references; GPU image decode is separate.
        views.append(dict(buffer=0, byteOffset=len(blob), byteLength=4))
        blob.extend(b'PNG!')
        doc['buffers'][0]['byteLength'] = len(blob)
        doc['images'] = [dict(bufferView=len(views) - 1, mimeType='image/png')]
        doc['textures'] = [dict(source=0)]
        doc['materials'] = [dict(pbrMetallicRoughness=dict(
            baseColorTexture=dict(index=0, texCoord=texture_uv)))]
        primitive['material'] = 0
        if pbr:
            slot = dict(index=0, texCoord=0, extensions={'KHR_texture_transform':
                        dict(offset=[.2, .3], scale=[2, 3], rotation=math.pi / 2, texCoord=1)})
            doc['extensionsUsed'] = ['KHR_texture_transform', 'KHR_materials_emissive_strength']
            doc['samplers'] = [dict(wrapS=33071, wrapT=33648, minFilter=9728, magFilter=9728)]
            doc['textures'][0]['sampler'] = 0
            doc['materials'] = [dict(pbrMetallicRoughness=dict(baseColorFactor=[.2, .3, .4, .6],
                metallicFactor=.7, roughnessFactor=.8, baseColorTexture=slot,
                metallicRoughnessTexture=slot), normalTexture=dict(**slot, scale=.4),
                occlusionTexture=dict(**slot, strength=.5), emissiveTexture=slot,
                emissiveFactor=[.1, .2, .3], alphaMode='MASK', alphaCutoff=.25,
                doubleSided=True, extensions={'KHR_materials_emissive_strength': {'emissiveStrength': 4}})]
    encoded = json.dumps(doc, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    blob += b'\0' * (-len(blob) % 4)
    payload = struct.pack('<III', 0x46546c67, 2, 28 + len(encoded) + len(blob))
    payload += struct.pack('<II', len(encoded), 0x4e4f534a) + encoded
    payload += struct.pack('<II', len(blob), 0x004e4942) + blob
    path.write_bytes(payload)


def write_fixtures(root):
    root.mkdir(parents=True, exist_ok=True)
    for name, options in (
        ('triangles', {}), ('flat', {'normals': False}),
        ('strip', {'mode': 5}), ('fan', {'mode': 6}),
        ('mirrored', {'mirrored': True}), ('color3', {'colors': 3}),
        ('invalid-index', {'invalid': True}),
        ('uv1-texture', {'texture_uv': 1}), ('missing-uv-texture', {'texture_uv': 0}),
        ('pbr', {'texture_uv': 1, 'pbr': True}),
        ('morph', {'morph': True}),
    ):
        fixture(root / f'{name}.glb', **options)
