"""Package checks use small deterministic assets; GPU/visual qualification is separate."""
from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('stage_tiles', ROOT / 'octaryn-client/Tools/MapImport/StageMapTiles.py')
STAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(STAGE)


class StageTiles(unittest.TestCase):
    def setUp(self):
        owner = ROOT / 'build/release-windows/tools/map-tile-stage-tests'
        owner.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=owner)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.tiles = self.root / 'cooked'
        self.bundle = self.root / 'bundle'
        (self.tiles / 'tiles').mkdir(parents=True)
        self.source = self.root / 'main.glb'
        self.source.write_bytes(b'reference fixture')
        self.source_hash = STAGE.digest(self.source)
        self.tile = self.tiles / 'tiles/0.glb'
        data = dict(buffers=[dict(byteLength=4)], accessors=[dict(count=3)],
                    meshes=[dict(primitives=[dict(indices=0)])])
        encoded = json.dumps(data).encode()
        encoded += b' ' * (-len(encoded) % 4)
        self.tile.write_bytes(struct.pack('<5I', 0x46546c67, 2, 32 + len(encoded), len(encoded), 0x4e4f534a) +
                             encoded + struct.pack('<2I', 4, 0x004e4942) + bytes(4))
        self.tile_hash = STAGE.digest(self.tile)
        self.manifest = dict(version=1, map='tiles/0.glb', tiles=[[0, 0, 0, 1, 1, 1]],
                             tile_files=['tiles/0.glb'], spawn=[0, 2, 0], yaw=0, pitch=0, texture_cache='textures')
        self.cooked = dict(version=1, source_sha256=self.source_hash, triangles=1, maximum_tile_triangles=1)
        self.verified = dict(version=1, source_sha256=self.source_hash, triangles=1, maximum_tile_triangles=1,
                             scope='positions_uv_colors_winding_dual64', files={'tiles/0.glb': self.tile_hash})
        lod = struct.pack('<8I', 0x444f4c4d, 2, 3, 3, 1, 0, 0, 0) + self.tile_hash.encode() + bytes(24)
        self.lod = self.tile.with_suffix('.glb.lods')
        self.lod.write_bytes(lod)
        self.lod.with_suffix('.lods.sha256').write_text(hashlib.sha256(lod).hexdigest())
        self.cache = self.bundle / 'main.glb.textures'
        self.cache.mkdir(parents=True)
        self.key = 'a' * 64
        self.dds = self.cache / (self.key + '.dds')
        self.dds.write_bytes(bytes(152))
        self.dds.with_suffix('.dds.sha256').write_text(STAGE.digest(self.dds))
        (self.cache / 'map-texture-cook.json').write_text(json.dumps(dict(
            version=3, status='complete', map_sha256=self.source_hash, files=[self.key])))

    def run_stage(self):
        for name, data in [('map.json', self.manifest), ('cook.json', self.cooked), ('verify.json', self.verified)]:
            (self.tiles / name).write_text(json.dumps(data))
        STAGE.stage(self.tiles, self.source, self.bundle)

    def rejected(self, pattern):
        with self.assertRaisesRegex(ValueError, pattern):
            self.run_stage()
        self.assertFalse((self.bundle / 'hq200.json').exists())
        self.assertFalse((self.bundle / 'hq200').exists())

    def test_complete_shares_reference_cache(self):
        self.run_stage()
        manifest = json.loads((self.bundle / 'hq200.json').read_text())
        self.assertEqual(manifest['texture_cache'], 'main.glb.textures')
        self.assertEqual(manifest['map'], 'hq200/tiles/0.glb')
        self.assertEqual((self.bundle / manifest['map']).read_bytes(), self.tile.read_bytes())
        self.assertFalse((self.bundle / 'hq200/textures').exists())

    def test_source_mismatch(self):
        self.cooked['source_sha256'] = '0' * 64
        self.rejected('source mismatch')

    def test_changed_glb(self):
        self.tile.write_bytes(self.tile.read_bytes()[:-1] + b'x')
        self.rejected('changed after native verification')

    def test_missing_receipt_coverage(self):
        self.verified['files'] = {}
        self.rejected('file coverage')

    def test_escape(self):
        self.manifest['map'] = self.manifest['tile_files'][0] = '../outside.glb'
        self.verified['files'] = {'../outside.glb': self.tile_hash}
        self.rejected('payload path')

    def test_corrupt_lod(self):
        self.lod.write_bytes(self.lod.read_bytes()[:-1] + b'x')
        self.rejected('LOD integrity')

    def test_stale_lod_generation(self):
        payload = bytearray(self.lod.read_bytes())
        struct.pack_into('<I', payload, 4, 1)
        self.lod.write_bytes(payload)
        self.lod.with_suffix('.lods.sha256').write_text(STAGE.digest(self.lod))
        self.rejected('LOD source metadata')

    def test_missing_texture(self):
        self.dds.unlink()
        self.rejected('cooked texture missing')

    def test_corrupt_texture(self):
        self.dds.write_bytes(b'x' + self.dds.read_bytes()[1:])
        self.rejected('texture integrity')

    def test_invalid_bounds(self):
        self.manifest['tiles'][0][0] = float('nan')
        self.rejected('invalid tile bounds')

    def test_conservation_mismatch(self):
        self.cooked['triangles'] = 2
        self.rejected('conservation metadata')


if __name__ == '__main__':
    unittest.main()
