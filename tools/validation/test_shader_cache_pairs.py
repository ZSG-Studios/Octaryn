"""Reject contamination, missing activation and identity drift in engine-cache pairs."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from shader_cache_report import cache_snapshot, cache_counters, load_case, validate_pair


class ShaderCachePairs(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.empty = cache_snapshot(self.directory)
        (self.directory / 'abc.bin').write_bytes(b'shader cache payload')
        self.populated = cache_snapshot(self.directory)
        self.first = self.case('engine_cache_empty', self.empty, hits=2, misses=4, writes=4)
        self.reused = self.case('engine_cache_reused', self.populated, hits=6, misses=0, writes=0)

    def case(self, state, before, **counters):
        return dict(identity=dict(executable='exact hash', shaders='exact hashes', driver='driver1',
                                  asset='asset1', settings={'mode': 0}), namespace='rhi-version',
            counters={kind:dict(counters) for kind in ('shaders','pipelines')},
            cache=dict(directory=str(self.directory), initial_files=before['files'], initial_bytes=before['bytes']),
            entry=dict(state=state, cache_directory=str(self.directory), before=before, after=self.populated))

    def test_empty_then_exact_reuse_accepted(self):
        validate_pair(self.first, self.reused)
        # A reused cache can still have misses: it is never labeled fully warm.
        self.reused['counters']['shaders']['misses'] = 1
        validate_pair(self.first, self.reused)

    def test_exact_hash_and_settings_changes_fail(self):
        for key in self.reused['identity']:
            altered = copy.deepcopy(self.reused)
            altered['identity'][key] = 'changed'
            with self.subTest(key=key), self.assertRaises(ValueError): validate_pair(self.first, altered)
        altered = copy.deepcopy(self.reused)
        altered['namespace'] = 'other backend namespace'
        with self.assertRaises(ValueError): validate_pair(self.first, altered)

    def test_same_length_content_tampering_fails(self):
        (self.directory / 'abc.bin').write_bytes(b'Shader cache payload')
        altered = copy.deepcopy(self.reused)
        altered['entry']['before'] = cache_snapshot(self.directory)
        self.assertEqual(altered['entry']['before']['bytes'], self.populated['bytes'])
        with self.assertRaises(ValueError): validate_pair(self.first, altered)

    def test_nonempty_first_wrong_directory_and_capture_mismatch_fail(self):
        with self.assertRaises(ValueError): cache_snapshot(self.directory / 'abc.bin')
        contaminated = copy.deepcopy(self.first)
        contaminated['entry']['before'] = self.populated
        with self.assertRaises(ValueError): validate_pair(contaminated, self.reused)
        for field, value in (('directory', str(self.directory / 'user-cache')), ('initial_files', 0), ('initial_bytes', 0)):
            altered = copy.deepcopy(self.reused)
            altered['cache'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): validate_pair(self.first, altered)

    def test_actual_miss_write_and_hit_evidence_required(self):
        for kind in ('shaders','pipelines'):
            for field in ('misses','writes'):
                first = copy.deepcopy(self.first);first['counters'][kind][field] = 0
                with self.subTest(kind=kind,field=field), self.assertRaises(ValueError): validate_pair(first, self.reused)
            reused = copy.deepcopy(self.reused);reused['counters'][kind]['hits'] = 0
            with self.assertRaises(ValueError): validate_pair(self.first, reused)

    def test_activation_counter_integrity_and_duplicate_lifetimes(self):
        line = 'hits=2 misses=1 writes=1 rejected=0 bytes=32 entries=1 evictions=0 evicted_bytes=0 write_refused=0 io_errors=0 lock_unavailable=0 entries_fresh=1'
        text = 'shader_cache explicit_directory=1\nshader_cache sdk_keys=1 shaders=1 pipelines=1 namespace=rhi-version\n'
        text += ''.join(f'shader_cache_summary kind={kind} {line}\n' for kind in ('shaders','pipelines'))
        self.assertEqual(cache_counters(text)['counters']['pipelines']['writes'], 1)
        for bad in (text.replace('explicit_directory=1','explicit_directory=0'),
                    text.replace('io_errors=0','io_errors=1'), text.replace('pipelines=1','pipelines=0'),
                    text+f'shader_cache_summary kind=shaders {line}\n'):
            with self.assertRaises(ValueError): cache_counters(bad)

    def test_windows_recorded_server_paths_are_supported(self):
        result = dict(exit_code=0, status='measured', map=dict(sha256='map-hash',
            cooked_identity=dict(texture_receipts={'count': 1, 'sha256': 'receipt-hash'})))
        build = dict(host=dict(graphics_adapters=['adapter-driver']), sha256={
            'Octaryn.Client.exe': 'client-hash', 'Client\\Shaders\\WorldMap.slang': 'shader-hash',
            'server\\Octaryn.Server.exe': 'server-hash'})
        (self.directory / 'result.json').write_text(json.dumps(result))
        (self.directory / 'client-build.json').write_text(json.dumps(build))
        fields = 'hits=2 misses=1 writes=1 rejected=0 bytes=32 entries=1 evictions=0 evicted_bytes=0 write_refused=0 io_errors=0 lock_unavailable=0 entries_fresh=1'
        text = 'authoritative_player_ready eye=0,2,0\nshader_cache explicit_directory=1\n'
        text += 'shader_cache sdk_keys=1 shaders=1 pipelines=1 namespace=rhi-version\n'
        text += ''.join(f'shader_cache_summary kind={kind} {fields}\n' for kind in ('shaders','pipelines'))
        (self.directory / 'client.log').write_text(text)
        self.assertTrue(load_case({'case':str(self.directory)})['loading']['authoritative_ready_observed'])


if __name__ == '__main__':
    unittest.main()
