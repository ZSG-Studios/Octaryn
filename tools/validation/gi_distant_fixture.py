"""View the authored courtyard beyond the near GI receiver window."""
import argparse
import json
from pathlib import Path
from gi_quality_fixture import prepare as courtyard, write_json


def prepare(bundle, output):
    courtyard(bundle, output)
    world = output / 'world'
    pose = json.loads((world / 'player_1.json').read_text())
    pose.update(z=64.5, pitch=0)
    write_json(world / 'player_1.json', pose)
    saved = json.loads((world / 'world_blocks.json').read_text())
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {row['id'].rsplit('.', 1)[-1]: i for i, row in enumerate(catalog)}
    # A separate open platform keeps normal spawn recovery at the authored eye.
    for x in range(-3, 4):
        for z in range(61, 68):
            for y in range(160, 177):
                saved['blocks'].append(dict(x=x, y=y, z=z, block=ids['stone' if y == 160 else 'air']))
    write_json(world / 'world_blocks.json', saved)
    metadata = json.loads((output / 'fixture.json').read_text())
    metadata.update(name='distant-courtyard-v1', pose=pose, regions={}, roi_confirmation=None,
        authored_edits=len(saved['blocks']), receiver_distance_metres=81.5,
        required_receiver=dict(min=[-10, 161, -17], max=[10, 167, -17], direction=5),
        correctness_scope='Actual primary receivers on the courtyard back wall are 81.5 m from the camera, outside the 32 m near admission radius. Inspect images and exact world-space chart data; no inherited pixel regions or radiometric truth.')
    write_json(output / 'fixture.json', metadata)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.client_bundle_root.resolve(), args.output.resolve()))
