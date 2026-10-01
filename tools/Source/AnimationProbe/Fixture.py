"""Exercise the animated glTF importer with an original deterministic fixture."""
import base64
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def fixture():
    binary = bytearray()
    views, accessors = [], []

    def add(values, kind, count, component=5126, **extra):
        while len(binary) % 4:
            binary.append(0)
        offset = len(binary)
        binary.extend(struct.pack('<' + ('f' if component == 5126 else 'H') * len(values), *values))
        views.append(dict(buffer=0, byteOffset=offset, byteLength=len(binary) - offset))
        accessors.append(dict(bufferView=len(views) - 1, componentType=component, count=count, type=kind, **extra))
        return len(accessors) - 1

    position = add([1, 0, 0, 0, 0, 0, 0, 1, 0], 'VEC3', 3, min=[0, 0, 0], max=[1, 1, 0])
    normal = add([0, 0, 1] * 3, 'VEC3', 3)
    joints = add([0] * 12, 'VEC4', 3, 5123)
    weights = add([1, 0, 0, 0] * 3, 'VEC4', 3)
    morph = add([2, 0, 0] * 3, 'VEC3', 3, min=[2, 0, 0], max=[2, 0, 0])
    times = add([0, 2], 'SCALAR', 2, min=[0], max=[2])
    translation = add([0, 0, 0, 0, 4, 0], 'VEC3', 2)
    morph_weights = add([0, 1], 'SCALAR', 2)
    inverse_bind = add([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], 'MAT4', 1)
    asset = dict(
        asset={'version': '2.0', 'generator': 'ZSG animation fixture'},
        scene=0, scenes=[{'nodes': [0]}],
        nodes=[{'translation': [3, 0, 0], 'children': [1, 2]}, {'mesh': 0, 'skin': 0}, {}],
        meshes=[{'weights': [0], 'primitives': [{'attributes': {
            'POSITION': position, 'NORMAL': normal, 'JOINTS_0': joints, 'WEIGHTS_0': weights},
            'targets': [{'POSITION': morph}]}]}],
        skins=[{'joints': [2], 'inverseBindMatrices': inverse_bind}],
        animations=[{'samplers': [
            {'input': times, 'output': translation, 'interpolation': 'LINEAR'},
            {'input': times, 'output': morph_weights, 'interpolation': 'LINEAR'}],
            'channels': [{'sampler': 0, 'target': {'node': 2, 'path': 'translation'}},
                         {'sampler': 1, 'target': {'node': 1, 'path': 'weights'}}]}],
        buffers=[{'byteLength': len(binary), 'uri': 'fixture.bin'}], bufferViews=views, accessors=accessors)
    return asset, binary, times


def main():
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='zsg-animation-') as temporary:
        directory = Path(temporary)
        asset, binary, times = fixture()
        path = directory / 'fixture.gltf'
        (directory / 'fixture.bin').write_bytes(binary)
        path.write_text(json.dumps(asset))
        subprocess.run([str(executable), str(path), '--fixture'], check=True)
        # Exercise embedded buffers through the same accessor parser.
        embedded = copy.deepcopy(asset)
        embedded['buffers'][0]['uri'] = 'data:application/octet-stream;base64,' + base64.b64encode(binary).decode()
        path.write_text(json.dumps(embedded))
        subprocess.run([str(executable), str(path), '--fixture'], check=True)
        # A repeated timestamp must fail without replacing a valid output asset.
        malformed = bytearray(binary)
        offset = asset['bufferViews'][asset['accessors'][times]['bufferView']]['byteOffset']
        struct.pack_into('<f', malformed, offset + 4, 0)
        (directory / 'fixture.bin').write_bytes(malformed)
        path.write_text(json.dumps(asset))
        result = subprocess.run([str(executable), str(path)], capture_output=True, text=True, encoding='utf-8')
        if result.returncode == 0 or 'animation times must increase' not in result.stderr:
            raise RuntimeError('invalid animation timestamp accepted')
        print('ANIMATION_IMPORT_FIXTURE PASS external embedded midpoint invalid-timestamp')


if __name__ == '__main__':
    main()
