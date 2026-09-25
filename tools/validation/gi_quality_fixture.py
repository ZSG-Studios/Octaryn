"""Authored world-space courtyard with image-confirmed measurement regions."""
import argparse
import json
import math
from pathlib import Path


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def pixel_region_box(box):
    # Measurements truncate normalized coordinates; guard against float roundoff
    # turning an exact end-exclusive edge such as 509/960 back into pixel 508.
    return [math.nextafter(value / extent, math.inf)
            for value, extent in zip(box, (960, 540, 960, 540))]


def prepare(bundle, output):
    output.mkdir(parents=True, exist_ok=False)
    world = output / 'world'
    world.mkdir()
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {row['id'].rsplit('.', 1)[-1]: index for index, row in enumerate(catalog)}
    blocks = {}

    def box(x0, x1, y0, y1, z0, z1, material):
        value = ids[material]
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    blocks[x, y, z] = value

    # An elevated authored volume separates the receivers from generated terrain.
    box(-14, 14, 160, 160, -19, 17, 'stone')
    box(-14, 14, 161, 176, -19, 17, 'air')
    box(-12, 12, 160, 160, -17, 15, 'planks')
    box(-10, 10, 160, 160, -16, 13, 'stone')
    # Neutral back wall, wood frame and one-metre side walls. The wide entrance
    # stays open so a normal outside spawn can see actual shaded indoor surfaces.
    box(-10, 10, 161, 167, -17, -17, 'snow')
    box(-10, -10, 161, 167, -16, -2, 'stone')
    box(10, 10, 161, 167, -16, -2, 'stone')
    box(-10, 10, 168, 168, -17, -3, 'planks')
    for x in (-10, -5, 5, 10):
        box(x, x, 161, 168, -3, -3, 'log')
    box(-10, 10, 168, 168, -3, -2, 'log')
    # A skylight lights a pale receiver beside a green planted surface. The
    # neighboring ceiling remains closed, exposing a direct-to-indirect transition.
    box(-4, -1, 168, 168, -12, -8, 'air')
    box(-4, -1, 160, 160, -12, -8, 'snow')
    box(-6, -6, 161, 164, -13, -8, 'grass')
    box(-4, 4, 161, 161, -16, -16, 'stone')
    for x, name in ((-7, 'red_torch'), (7, 'blue_torch')):
        box(x, x, 161, 161, -12, -12, name)
        box(x - 1, x + 1, 160, 160, -14, -10, 'snow')
    # One-metre partition between colored bays, with a real open route around it.
    box(0, 0, 161, 164, -16, -13, 'stone')
    # Contained water is the production reflective material; no test-only shader.
    box(-4, 4, 159, 159, 1, 7, 'stone')
    box(-4, 4, 160, 160, 1, 7, 'stone')
    box(-3, 3, 160, 160, 2, 6, 'water')
    for side in (-1, 1):
        x0, x1 = sorted((side * 8, side * 11))
        box(x0, x1, 160, 160, 2, 9, 'grass')
        for z, plant in ((3, 'bush'), (5, 'gardenia'), (7, 'rose')):
            box(side * 9, side * 9, 161, 161, z, z, plant)
        box(side * 11, side * 11, 161, 164, 0, 0, 'log')
        box(side * 11 - 1, side * 11 + 1, 165, 166, -1, 1, 'leaves')
    pose = dict(version=1, x=.5, y=162.62, z=13.5, yaw=0, pitch=-.06, block=ids['stone'])
    write_json(world / 'world_blocks.json', dict(version=1, blocks=[
        dict(x=x, y=y, z=z, block=value) for (x, y, z), value in sorted(blocks.items())]))
    write_json(world / 'world_generation.json', dict(version=1, generator='octaryn.basegame',
                                                    revision=3, seed=1337, mode=0))
    write_json(world / 'world_time.json', dict(version=1, day_index=0, seconds_of_day=9 * 3600))
    write_json(world / 'player_1.json', pose)
    write_json(output / 'settings.json', dict(version=15, windowWidth=960, windowHeight=540,
        fullscreen=False, renderDistance=4, upscalerMode=1, frameCapFps=30,
        rayTracingEnabled=True, shadowQuality=3, reflectionQuality=3,
        shadowDistance=1024, reflectionDistance=1024, pbrEnabled=True, pomEnabled=True,
        fogEnabled=True, cloudsEnabled=False, fsrSharpness=.2))
    write_json(output / 'lighting.json', dict(version=1, ambient_strength=.65,
        sun_strength=.75, sun_fallback_strength=.75, fog_distance=1024, skylight_floor=.25))
    # End-exclusive boxes confirmed in the 960x540 reference below. An independent
    # camera-ray walk through authored blocks also found one face class per box.
    pixel_regions = dict(red_receiver=[438, 220, 466, 246], blue_receiver=[514, 220, 542, 246],
                         neutral_back_wall=[488, 209, 509, 223], water=[432, 301, 527, 312],
                         outdoor_stone=[240, 354, 340, 392])
    regions = {name: pixel_region_box(box) for name, box in pixel_regions.items()}
    receivers = dict(
        red_receiver=dict(material='snow', normal=[0, 0, 1], block_min=[-5, 163, -17], block_max=[-2, 166, -17]),
        blue_receiver=dict(material='snow', normal=[0, 0, 1], block_min=[4, 163, -17], block_max=[7, 166, -17]),
        neutral_back_wall=dict(material='snow', normal=[0, 0, 1], block_min=[1, 166, -17], block_max=[3, 167, -17]),
        water=dict(material='water', normal=[0, 1, 0], block_min=[-2, 160, 4], block_max=[2, 160, 5]),
        outdoor_stone=dict(material='stone', normal=[0, 1, 0], block_min=[-4, 160, 9], block_max=[-2, 160, 10]))
    write_json(output / 'fixture.json', dict(name='one-metre-courtyard-v2', pose=pose,
        authored_edits=len(blocks), regions=regions,
        roi_visual_confirmation='Reference image manually inspected; reconfirm after camera, scene or projection changes.',
        roi_confirmation=dict(
            reference_capture='logs/client/block-transport-world-validation/block-transport-block-transport-4lk9xvdt/frame.bmp.sample-2.bmp',
            reference_sha256='a38386be4e2b4895d6e5e5242202cdb42d8bb241f4ccb1411a7b9f357c59ba11',
            reference_frame=350, reference_resolution=[960, 540],
            camera=dict(eye=[.5, 162.620010, 13.5], yaw=0, pitch=-.06, vertical_fov_radians=1.570796),
            pixel_boxes=pixel_regions, authored_world_receivers=receivers,
            method='Manual reference image inspection plus independent ideal camera-ray walk through authored voxel edits; no GPU material-ID readback.',
            limits='Boxes are display-RGB measurements of visible surfaces, not masks isolating indirect radiance. Water also contains animated reflection and material shading.',
            omitted_skylight_receiver='Visible floor is too thin and partially occluded for a pure stable box at this camera; skylight geometry remains authored.'),
        authored_world=dict(ground_y=160, back_wall_z=-17, ceiling_y=168,
            skylight=dict(min=[-4, 168, -12], max=[-1, 168, -8]),
            lights=[dict(material='red_torch', cell=[-7, 161, -12]),
                    dict(material='blue_torch', cell=[7, 161, -12])],
            intent='The saved world defines geometry and emitters independently of camera or screen resolution.'),
        features=['one-metre walls', 'partially roofed neutral receiver', 'green bounce surface',
                  'red and blue torches', 'water reflection basin', 'outdoor vegetation'],
        correctness_scope='This open room measures rendered appearance only. It is not a sealed-wall leakage oracle, radiometric ground truth, automatic visual acceptance or proof of convergence.'))
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.client_bundle_root.resolve(), args.output.resolve()))
