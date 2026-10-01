"""Generate an original mirror fixture and inspect actual captured GPU colors."""
import argparse
import json
import math
from pathlib import Path
import re
import struct


def generate(directory):
    directory.mkdir(parents=True, exist_ok=True)
    blob = bytearray()
    views, accessors, primitives = [], [], []

    def attribute(values, kind, components, component_type=5126):
        while len(blob) % 4:
            blob.append(0)
        first = len(blob)
        code = 'f' if component_type == 5126 else 'I'
        for value in values:
            blob.extend(struct.pack('<' + code * components, *value))
        views.append(dict(buffer=0, byteOffset=first, byteLength=len(blob)-first))
        item = dict(bufferView=len(views)-1, componentType=component_type,
                    count=len(values), type=kind)
        if kind == 'VEC3':
            item.update(min=[min(v[i] for v in values) for i in range(3)],
                        max=[max(v[i] for v in values) for i in range(3)])
        accessors.append(item)
        return len(accessors)-1

    def quad(points, normal, material):
        position = attribute(points, 'VEC3', 3)
        normals = attribute([normal] * 4, 'VEC3', 3)
        indices = attribute([(i,) for i in (0, 1, 2, 0, 2, 3)], 'SCALAR', 1, 5125)
        primitives.append(dict(attributes=dict(POSITION=position, NORMAL=normals),
                               indices=indices, material=material))

    quad([(-8, 0, 12), (8, 0, 12), (8, 0, -12), (-8, 0, -12)], (0, 1, 0), 0)
    quad([(-2.2, 2.6, 0), (-.2, 2.6, 0), (-.2, 4.2, 0), (-2.2, 4.2, 0)], (0, 0, 1), 1)
    quad([(.2, 2.6, 0), (2.2, 2.6, 0), (2.2, 4.2, 0), (.2, 4.2, 0)], (0, 0, 1), 2)
    # Transparent authored mask sits in front of both offscreen emissive panels.
    quad([(-2.2, 2.6, .1), (2.2, 2.6, .1), (2.2, 4.2, .1), (-2.2, 4.2, .1)], (0, 0, 1), 3)
    materials = [dict(name='Mirror', doubleSided=True, pbrMetallicRoughness=dict(
        baseColorFactor=[.92, .92, .92, 1], metallicFactor=1, roughnessFactor=.08))]
    for name, color, alpha in [('Red', [1, 0, 0], 1), ('Green', [0, 1, 0], 1), ('Masked blue', [0, 0, 1], 0)]:
        material = dict(name=name, doubleSided=True,
                        pbrMetallicRoughness=dict(baseColorFactor=color+[alpha], metallicFactor=0, roughnessFactor=.7),
                        emissiveFactor=color, extensions={'KHR_materials_emissive_strength': {'emissiveStrength': 4}})
        if alpha == 0:
            material.update(alphaMode='MASK', alphaCutoff=.5)
        materials.append(material)
    document = dict(asset=dict(version='2.0', generator='ZSG reflection fixture'),
                    extensionsUsed=['KHR_materials_emissive_strength'],
                    buffers=[dict(uri='reflection.bin', byteLength=len(blob))],
                    bufferViews=views, accessors=accessors, materials=materials,
                    meshes=[dict(primitives=primitives)], nodes=[dict(mesh=0)],
                    scenes=[dict(nodes=[0])], scene=0)
    (directory/'reflection.bin').write_bytes(blob)
    (directory/'reflection.gltf').write_text(json.dumps(document), encoding='utf-8')
    (directory/'map.json').write_text(json.dumps(dict(version=1, map='reflection.gltf',
        spawn=[0, 1.62, 6], yaw=0, pitch=-.85)), encoding='utf-8')
    (directory/'fixture.json').write_text(json.dumps(dict(camera=[0, 2, 6, 0, -.85],
        floor_roughness=.08, floor_metallic=1, expected=['offscreen red reflection', 'offscreen green reflection',
        'no masked blue panel reflection'], license='CC0-1.0', triangles=8)), encoding='utf-8')
    return directory/'map.json'


def offscreen_evidence(path, origin_x=0):
    observation = json.loads(Path(str(path)+'.observation.json').read_text())
    log = (path.parent/'client.log').read_text(encoding='utf-8', errors='replace')
    matches = re.findall(r'^world_capture frame=(\d+) nonclear_pixels=\d+ eye=([\d.eE+,-]+) '
                         r'yaw=([\d.eE+-]+) pitch=([\d.eE+-]+) fov=([\d.eE+-]+) path=', log, re.M)
    matches = [m for m in matches if int(m[0]) == observation['frame']]
    if len(matches) != 1:
        raise AssertionError('Missing unique actual capture camera/FOV evidence')
    _, eye_text, yaw, pitch, fov = matches[0]
    eye = [float(v) for v in eye_text.split(',')]
    yaw, pitch, fov = map(float, (yaw, pitch, fov))
    if len(eye) != 3 or not all(math.isfinite(v) for v in eye+[yaw, pitch, fov]) or not 0 < fov < math.pi:
        raise AssertionError('Invalid actual capture camera/FOV')
    sy, cy, sp, cp = math.sin(yaw), math.cos(yaw), math.sin(pitch), math.cos(pitch)
    up, forward = (-sy*sp, cp, cy*sp), (sy*cp, sp, -cy*cp)
    clearances = []
    for x in (-2.2, 2.2):
        for y in (2.6, 4.2):
            relative = [origin_x+x-eye[0], y-eye[1], -eye[2]]
            depth = sum(v*f for v, f in zip(relative, forward))
            clearance = sum(v*u for v, u in zip(relative, up))-depth*math.tan(fov/2)
            if depth <= 0 or clearance <= .01:
                raise AssertionError(f'Emissive panel enters actual camera frustum: fov={fov} pitch={pitch}')
            clearances.append(clearance)
    return dict(frame=observation['frame'], camera=eye+[yaw, pitch], fov_radians=fov,
                minimum_above_frustum=min(clearances))


def inspect(path, expect_reflections, origin_x=0):
    offscreen = offscreen_evidence(path, origin_x)
    data = path.read_bytes()
    if data[:2] != b'BM':
        raise ValueError('Capture must be BMP')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    if bits not in (24, 32) or width <= 0 or not height:
        raise ValueError('Unsupported captured BMP')
    stride = ((width*bits+31)//32)*4
    counts = dict(red=0, green=0, blue=0)
    for y in range(abs(height)//3, abs(height)):
        row = abs(height)-1-y if height > 0 else y
        for x in range(width):
            start = offset+row*stride+x*(bits//8)
            b, g, r = data[start:start+3]
            for name, value, a, c in [('red', r, g, b), ('green', g, r, b), ('blue', b, r, g)]:
                if value > 70 and value > max(a, c)*1.65:
                    counts[name] += 1
    minimum = max(20, width*abs(height)//1500)
    if expect_reflections and (counts['red'] < minimum or counts['green'] < minimum):
        raise AssertionError(f'Offscreen reflected panels missing: {counts}')
    if not expect_reflections and (counts['red'] >= minimum or counts['green'] >= minimum):
        raise AssertionError(f'Colored panels visible with reflections disabled: {counts}')
    if counts['blue'] >= minimum:
        raise AssertionError(f'Masked blue panel reflected: {counts}')
    return dict(path=str(path), colored_pixels=counts, expected_reflections=expect_reflections,
                offscreen_evidence=offscreen)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generate', type=Path)
    parser.add_argument('--capture', type=Path)
    parser.add_argument('--reflections', choices=('on', 'off'), default='on')
    args = parser.parse_args()
    if args.generate:
        print(generate(args.generate.resolve()))
    if args.capture:
        print(json.dumps(inspect(args.capture.resolve(), args.reflections == 'on'), indent=2))
