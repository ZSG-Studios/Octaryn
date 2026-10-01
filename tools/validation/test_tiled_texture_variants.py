"""Exercise dependency isolation and identity gates for tiled codec comparisons."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

from prepare_tiled_texture_variants import geometry_files, prepare


def digest(data):
    return hashlib.sha256(data).hexdigest()


class TiledVariantsTests(unittest.TestCase):
    def fixture(self, root, uri='../images/source.png'):
        maps = root / 'input'
        (maps / 'tiles').mkdir(parents=True)
        (maps / 'images').mkdir()
        (maps / 'images/source.png').write_bytes(b'image source')
        metadata = json.dumps(dict(asset=dict(version='2.0'), images=[dict(uri=uri)])).encode()
        metadata += b' ' * (-len(metadata) % 4)
        glb = struct.pack('<5I', 0x46546c67, 2, 20 + len(metadata), len(metadata), 0x4e4f534a) + metadata
        source = maps / 'tiles/a.glb'
        source.write_bytes(glb)
        lod = bytes(32) + digest(glb).encode()
        Path(str(source) + '.lods').write_bytes(lod)
        Path(str(source) + '.lods.sha256').write_text(digest(lod))
        manifest = maps / 'map.json'
        manifest.write_text(json.dumps(dict(version=1, map='tiles/a.glb', tile_files=['tiles/a.glb'],
                                            tiles=[[0, 0, 0, 1, 1, 1]], spawn=[0, 2, 0], yaw=0, pitch=0)))
        caches = []
        for name, codec in [('reference', 'rgba8-lossless'), ('candidate', 'bc7-opaque-color-uber4')]:
            cache = root / name
            cache.mkdir()
            key = 'a' * 64
            payload = bytes(148) + name.encode()
            (cache / (key + '.dds')).write_bytes(payload)
            (cache / (key + '.dds.sha256')).write_text(digest(payload))
            (cache / 'map-texture-cook.json').write_text(json.dumps(dict(
                version=3, status='complete', codec=codec, map_sha256=digest(glb), files=[key])))
            caches.append(cache)
        return manifest, source, *caches, root / 'variants'

    def test_same_geometry_view_and_external_images_different_codec(self):
        with tempfile.TemporaryDirectory() as folder:
            args = self.fixture(Path(folder))
            result = prepare(*args)
            a, b = (result['variants'][name] for name in ('lossless', 'bc7'))
            self.assertEqual(a['geometry_identity'], b['geometry_identity'])
            self.assertEqual(a['manifest_sha256'], b['manifest_sha256'])
            self.assertNotEqual(a['texture_payload_identity'], b['texture_payload_identity'])
            for variant in ('lossless', 'bc7'):
                self.assertEqual((args[-1] / variant / 'images/source.png').read_bytes(), b'image source')
                self.assertEqual((args[-1] / variant / 'tiles/a.glb').read_bytes(), args[1].read_bytes())
            self.assertEqual(len(result['geometry_files']), 4)

    def test_escape_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            args = self.fixture(Path(folder), '../../outside.png')
            (Path(folder) / 'outside.png').write_bytes(b'outside')
            with self.assertRaisesRegex(ValueError, 'escaping'):
                geometry_files(args[0])

    def test_corrupt_payload_and_stale_lod_rejected(self):
        for change in ('payload', 'lod'):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as folder:
                args = self.fixture(Path(folder))
                if change == 'payload':
                    (args[3] / ('a' * 64 + '.dds')).write_bytes(bytes(160))
                else:
                    Path(str(args[1]) + '.lods.sha256').write_text('0' * 64)
                with self.assertRaises(ValueError):
                    prepare(*args)

    def test_source_cache_and_output_isolation(self):
        with tempfile.TemporaryDirectory() as folder:
            args = self.fixture(Path(folder))
            with self.assertRaisesRegex(ValueError, 'separate'):
                prepare(*args[:-1], args[0].parent)
            metadata = args[3] / 'map-texture-cook.json'
            doc = json.loads(metadata.read_text())
            doc['map_sha256'] = '0' * 64
            metadata.write_text(json.dumps(doc))
            with self.assertRaisesRegex(ValueError, 'stale'):
                prepare(*args)


if __name__ == '__main__':
    unittest.main()
