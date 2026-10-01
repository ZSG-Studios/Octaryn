"""Build two original map tiles to exercise shared VG depth and asynchronous roots."""
import argparse
import json
from pathlib import Path

from reflection_fixture import generate


def generate_tiles(directory):
    files = []
    for index in range(2):
        folder = directory/str(index)
        generate(folder)
        source = folder/'reflection.gltf'
        document = json.loads(source.read_text())
        document['nodes'][0]['translation'] = [index*16, 0, 0]
        source.write_text(json.dumps(document), encoding='utf-8')
        files.append(str(source.relative_to(directory)).replace('\\', '/'))
    manifest = directory/'map.json'
    manifest.write_text(json.dumps(dict(version=1, map=files[0], tile_files=files,
        tiles=[[-8, 0, -12, 8, 4.2, 12], [8, 0, -12, 24, 4.2, 12]],
        spawn=[8, 1.62, 6], yaw=0, pitch=-.85)), encoding='utf-8')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    print(generate_tiles(parser.parse_args().directory.resolve()))
