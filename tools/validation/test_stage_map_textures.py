"""CPU-only checks that texture cache packaging excludes partial/stale cooks."""
from pathlib import Path
import hashlib
import importlib.util
import json
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'stage_map_textures', ROOT / 'octaryn-client/Tools/MapImport/StageMapTextures.py')
STAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(STAGE)


class StageTests(unittest.TestCase):
    def setUp(self):
        output = ROOT / 'build/release-windows/tools/map-texture-stage-test'
        output.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=output)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        self.cache = self.root / 'cache'
        self.bundle = self.root / 'bundle'
        self.source.mkdir()
        self.map = self.source / 'main.glb'
        self.map.write_bytes(b'fixture map source')
        self.directory = self.cache / 'main.glb.textures'
        self.directory.mkdir(parents=True)
        self.key = 'a' * 64
        self.dds = self.directory / (self.key + '.dds')
        self.dds.write_bytes(b'DDS ' + bytes(160))
        self.checksum = self.directory / (self.key + '.dds.sha256')
        self.checksum.write_text(hashlib.sha256(self.dds.read_bytes()).hexdigest())
        self.document = dict(version=2, status='complete',
                             map_sha256=STAGE.digest(self.map), files=[self.key])

    def run_stage(self):
        (self.directory / 'map-texture-cook.json').write_text(json.dumps(self.document))
        return STAGE.stage(self.cache, self.source, self.bundle)

    def test_complete(self):
        self.assertEqual(self.run_stage(), 1)
        self.assertEqual((self.bundle / self.directory.name / self.dds.name).read_bytes(), self.dds.read_bytes())

    def test_pilot(self):
        self.document['status'] = 'pilot'
        self.assertEqual(self.run_stage(), 0)
        self.assertFalse(self.bundle.exists())

    def test_changed_source(self):
        self.map.write_bytes(b'changed map')
        self.assertEqual(self.run_stage(), 0)
        self.assertFalse(self.bundle.exists())

    def test_old_version(self):
        self.document['version'] = 1
        self.assertEqual(self.run_stage(), 0)

    def test_corrupt(self):
        self.dds.write_bytes(b'DDS ' + bytes(159) + b'x')
        with self.assertRaisesRegex(ValueError, 'digest mismatch'):
            self.run_stage()
        self.assertFalse(self.bundle.exists())

    def test_missing_receipt(self):
        self.checksum.unlink()
        with self.assertRaisesRegex(ValueError, 'integrity receipt'):
            self.run_stage()

    def test_path_escape(self):
        self.document['files'] = ['../outside']
        with self.assertRaisesRegex(ValueError, 'key list'):
            self.run_stage()

    def test_duplicate(self):
        self.document['files'] *= 2
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.run_stage()


if __name__ == '__main__':
    unittest.main()
