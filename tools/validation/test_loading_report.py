"""Keep loading clocks and incomplete historical identity evidence distinct."""
import unittest

from asset_evidence import compare_recorded_asset_identities
from loading_report import parse_loading


class LoadingEvidence(unittest.TestCase):
    def test_stage_durations_are_not_cumulative_boot_milestones(self):
        result = parse_loading('\n'.join((
            'client_boot stage=renderer_ready elapsed_ms=1000',
            'map_startup stage=load elapsed_ms=400.5',
            'map_startup stage=images elapsed_ms=800.25',
            'map_model_loaded triangles=12 primitives=1 images=2 ms=1300.0',
            'map_boot responsiveness=1 worker_jobs=1 event_pumps=200 max_event_gap_ms=8.1 worker_elapsed_ms=1700 result=ready',
            'authoritative_player_ready eye=0,2,-20 yaw=0.6 pitch=-0.25')))
        self.assertEqual(result['measured']['map_images_ms'], 800.25)
        self.assertEqual(result['measured']['map_renderer_total_ms'], 1300)
        self.assertTrue(result['authoritative_ready_observed'])
        self.assertFalse(result['authoritative_ready_latency_available'])
        self.assertNotIn('authoritative_ready_elapsed_ms', result['measured'])

    def test_new_readiness_clocks_have_separate_names(self):
        result = parse_loading('authoritative_player_ready eye=0,2,-20 sdl_uptime_ms=6500 session_elapsed_ms=245.25')
        self.assertEqual(result['measured']['authoritative_ready_sdl_uptime_ms'], 6500)
        self.assertEqual(result['measured']['authoritative_ready_session_elapsed_ms'], 245.25)
        self.assertTrue(result['authoritative_ready_latency_available'])
        self.assertNotIn('process_launch_to_ready_ms', result['measured'])

    def test_multiple_loads_do_not_collapse_into_one_startup(self):
        result = parse_loading('map_startup stage=images elapsed_ms=4\nmap_startup stage=images elapsed_ms=8')
        self.assertEqual(result['map_stage_records_ms']['images'], [4, 8])
        self.assertNotIn('map_images_ms', result['measured'])

    def test_missing_historical_receipts_remain_missing(self):
        newer = {'cooked_identity': {'texture_cache': 'one', 'texture_receipts': {'sha256': 'a', 'count': 2}}}
        result = compare_recorded_asset_identities([{}, newer])
        self.assertEqual(result['cooked_identity_recorded_runs'], 1)
        self.assertFalse(result['complete_cooked_receipt_coverage'])

    def test_receipt_mismatch_rejected_even_when_metadata_is_unchanged(self):
        first = {'cooked_metadata': {'manifest': 'same'}, 'cooked_identity': {'texture_cache': 'one', 'texture_receipts': {'sha256': 'a', 'count': 2}}}
        second = {'cooked_metadata': {'manifest': 'same'}, 'cooked_identity': {'texture_cache': 'two', 'texture_receipts': {'sha256': 'b', 'count': 2}}}
        with self.assertRaisesRegex(ValueError, 'cooked_identity'):
            compare_recorded_asset_identities([first, second])
        second['cooked_identity']['texture_receipts']['sha256'] = 'a'
        self.assertTrue(compare_recorded_asset_identities([first, second])['complete_cooked_receipt_coverage'])


if __name__ == '__main__':
    unittest.main()
