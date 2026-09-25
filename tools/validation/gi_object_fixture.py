"""Create authoritative saved items viewed from a third-person room entrance."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import struct
from gi_quality_fixture import write_json


def prepare(source, bundle, output):
    if output.exists():
        raise ValueError('Object fixture output must be a new directory')
    shutil.copytree(source, output)
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {row['id'].rsplit('.', 1)[-1]: index for index, row in enumerate(catalog)}
    items = [dict(id=i + 1, material=material, position=[x, 161.3, z])
             for i, (material, x, z) in enumerate((('snow', -3, -8), ('snow', 3, -8),
                                                   ('rose', -2, -6), ('gardenia', 2, -6)))]
    state = bytearray(15440)
    struct.pack_into('<IIQQQQddIIIIII', state, 0, 1, len(state), len(items) + 1, 1, 0, 0,
                     0, 0, 0, 0, 0, len(items), 0, 0)
    for index, item in enumerate(items):
        struct.pack_into('<QII6f2d', state, 80 + index * 56, item['id'], ids[item['material']], 1,
                         *item['position'], 0, 0, 0, 0, 2)
    library = next(bundle.rglob('octaryn_server_world_items.dll'), None)
    if library is None:
        raise ValueError('Fixture requires the packaged native authoritative item validator')
    native = ctypes.CDLL(str(library))
    native.octaryn_items_validate.argtypes = [ctypes.c_void_p]
    native.octaryn_items_validate.restype = ctypes.c_int
    memory = ctypes.create_string_buffer(bytes(state))
    if native.octaryn_items_validate(memory) != 0:
        raise ValueError('Authoritative native validator rejected the authored item state')
    (output / 'world/world_items.bin').write_bytes(hashlib.sha256(state).digest() + state)
    # Ordinary spawn recovery chooses the highest floor at a roofed location.
    # The open entrance keeps the real player below the roof and the items visible.
    pose = dict(version=1, x=.5, y=162.62, z=-1.0, yaw=0, pitch=-.16, block=ids['stone'])
    write_json(output / 'world/player_1.json', pose)
    metadata = json.loads((output / 'fixture.json').read_text())
    metadata.update(name='entrance-object-lighting-v2', pose=pose, regions={}, items=items,
                    third_person=True, roi_confirmation=None,
                    correctness_scope='Actual player and rotating cube/sprite item rendering inside an authored room. Inspect images; no inherited courtyard pixel boxes or radiometric ground truth.')
    write_json(output / 'fixture.json', metadata)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.source.resolve(), args.client_bundle_root.resolve(), args.output.resolve()))
