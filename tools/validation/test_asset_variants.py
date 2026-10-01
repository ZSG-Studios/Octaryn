"""Asset comparisons allow codec receipts only; workload drift must fail."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace

from asset_variant_report import validate_pair, texture_upload
from capture_asset_variants import command


class AssetVariants(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        root = Path(self.directory.name)
        self.cases, self.variants = [], {}
        for codec in ('lossless', 'bc7'):
            path = root / codec
            path.mkdir()
            receipts = dict(sha256=codec, count=2, payloads_rehashed=False)
            self.variants[codec] = dict(source_sha256='geometry', texture_manifest_sha256=codec,
                captured_texture_receipts=receipts, variants=2, texture_payload_bytes=100,
                codec='rgba8-lossless' if codec == 'lossless' else 'bc7-opaque-color-uber4')
            result = dict(rt_reference=True, draw_mode='direct', lod_pixels=0,
                map=dict(sha256='geometry', manifest={'map': 'main.glb'}, cooked_identity=dict(
                    texture_receipts=receipts, texture_manifest_sha256=codec,
                    lod_receipts=dict(count=1, sha256='lod'))))
            build = dict(host=dict(graphics_adapters=[dict(Name='gpu', DriverVersion='1')]),
                         sha256={'Octaryn.Client.exe': 'exe', 'Client/Shaders/WorldMap.slang': 'shader'})
            (path / 'client-build.json').write_text(json.dumps(build))
            self.cases.append(dict(entry=dict(variant=codec, case=str(path)), result=result))

    def test_only_codec_identity_can_differ(self):
        validate_pair(self.cases, self.variants)
        for field, value in (('dimensions', [1, 2]), ('rt_history_search', True), ('fixed_sampling', True)):
            changed = copy.deepcopy(self.cases)
            changed[1]['result'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                validate_pair(changed, self.variants)

    def test_geometry_lod_and_shader_drift_rejected(self):
        changed = copy.deepcopy(self.cases)
        changed[1]['result']['map']['sha256'] = 'other'
        with self.assertRaises(ValueError): validate_pair(changed, self.variants)
        changed = copy.deepcopy(self.cases)
        changed[1]['result']['map']['cooked_identity']['lod_receipts']['sha256'] = 'other'
        with self.assertRaises(ValueError): validate_pair(changed, self.variants)
        path = Path(self.cases[1]['entry']['case']) / 'client-build.json'
        build = json.loads(path.read_text())
        build['sha256']['Client/Shaders/WorldMap.slang'] = 'other'
        path.write_text(json.dumps(build))
        with self.assertRaises(ValueError): validate_pair(self.cases, self.variants)

    def test_wrong_texture_receipt_or_upload_rejected(self):
        changed = copy.deepcopy(self.cases)
        changed[1]['result']['map']['cooked_identity']['texture_receipts']['sha256'] = 'other'
        with self.assertRaises(ValueError): validate_pair(changed, self.variants)
        case = Path(self.cases[1]['entry']['case'])
        good = 'map_textures variants=2 cached_bc7=1 cached_rgba=1 uncooked_rgba=0 shared_uploads=0 gpu_bytes=100\n'
        (case / 'client.log').write_text(good)
        self.assertEqual(texture_upload(case, self.variants['bc7'])['gpu_bytes'], 100)
        for bad in (good.replace('gpu_bytes=100', 'gpu_bytes=101'), good.replace('uncooked_rgba=0', 'uncooked_rgba=1')):
            (case / 'client.log').write_text(bad)
            with self.assertRaises(ValueError): texture_upload(case, self.variants['bc7'])

    def test_quality_and_timing_are_distinct_commands(self):
        args = SimpleNamespace(size='1440p', client_bundle_root=Path('.'), backend='dx12',
            mode=0, timeout=600, max_frame_ms=250, workload='motion')
        for quality in (False, True):
            observed = command(args, 'map.json', Path('output'), [0, 2, -20, .6, -.25], quality)
            self.assertEqual('--fixed-sampling' in observed, quality)
            self.assertEqual('--uncapped-fps' in observed, not quality)
            self.assertIn('--rt-reference', observed)
            self.assertNotIn('--rt-sparse', observed)
            self.assertEqual(observed[observed.index('--captures')+1], '8' if quality else '1')


if __name__ == '__main__':
    unittest.main()
