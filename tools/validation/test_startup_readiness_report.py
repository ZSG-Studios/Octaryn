"""An explicit readiness guard has a clock and scope; slow full residency is not that gate."""
import unittest
from startup_readiness_report import initial_playable_report, readiness_activation

ACTIVATION = ('startup_readiness_profile enabled=1 clock_origin=main_entry '
              'cadence=every_successful_world_present cpu_scope=frame_total\n')

def inspect(log):
    return initial_playable_report(ACTIVATION + log)
from tile_performance_report import parse_tile_work


def event(marker='world_initial_playable', **changes):
    fields = dict(schema=1, clock_origin='main_entry', elapsed_ms=4000, renderer_frame=200,
                  session=0, authority_ack=100, requested_generation=17, requested_set_hash=18446744073709551615,
                  requested_tiles=2, resident_tiles=17, total_tiles=17, visible_tiles=2, visible_missing=0,
                  collision_ready=1, requested_ready=1, ray_required=1, ray_ready=1, ray_guard_complete=1,
                  scope='all_manifest_rt_guard', actor_x=0, actor_y=2, actor_z=-9,
                  camera_x=0, camera_y=3, camera_z=-9, yaw=.6, pitch=-.25, fov=1.57)
    fields.update(changes)
    return marker + ' ' + ' '.join(f'{key}={value}' for key, value in fields.items())


class InitialPlayabilityTests(unittest.TestCase):
    def test_absence_and_late_fullmap_do_not_decide_initial_playability(self):
        log = ('authoritative_player_ready session_elapsed_ms=1200\n'
               'tile_published id=0 readiness_ms=22561.512 collision_ready=1 ray_ready=1\n'
               'tile_stream resident=1 wanted=1 preparing=0 uploading=0\n')
        result = parse_tile_work(log, 1)
        self.assertTrue(result['full_map_residency']['full_manifest_ready_observed'])
        self.assertEqual(result['full_map_residency']['request_span_lower_bound_ms'], 22561.512)
        self.assertEqual(result['initial_playable_startup']['app_main_budget_pass'], {'5000': None, '8000': None})
        self.assertIsNone(result['initial_playable_startup']['process_launch_to_ready_ms'])

    def test_timely_final_guard_proves_only_app_main_bound(self):
        result = inspect(event())
        self.assertEqual(result['app_main_budget_pass'], {'5000': True, '8000': True})
        self.assertEqual(result['clock_origin'], 'main_entry')
        self.assertEqual(result['initial_event']['requested_set_hash'], (1 << 64)-1)
        self.assertIsNone(result['process_launch_to_ready_ms'])
        self.assertEqual(inspect(event(elapsed_ms=7000))['app_main_budget_pass'],
                         {'5000': None, '8000': True})

    def test_late_conservative_guard_does_not_prove_earliest_failure(self):
        result = inspect(event(elapsed_ms=22000))
        self.assertTrue(result['required_visible_collision_ready_observed'])
        self.assertEqual(result['app_main_budget_pass'], {'5000': None, '8000': None})
        self.assertEqual(result['startup_budget_disposition'],
                         'conservative_readiness_gate_exceeded_initial_playable_unqualified')

    def test_candidate_cannot_qualify_rt_guard(self):
        candidate = event('world_initial_playable_candidate', resident_tiles=2, ray_ready=0, ray_guard_complete=0)
        result = inspect(candidate)
        self.assertFalse(result['required_visible_collision_ready_observed'])
        self.assertEqual(result['startup_budget_disposition'], 'unqualified')
        final = event(elapsed_ms=7000, renderer_frame=300)
        result = inspect(candidate + '\n' + final)
        self.assertEqual(result['app_main_to_conservative_gate_ms'], 7000)

    def test_non_rt_requested_region_scope_is_explicit(self):
        result = inspect(event(ray_required=0, scope='visible_requested_region', resident_tiles=2))
        self.assertEqual(result['initial_event']['scope'], 'visible_requested_region')
        self.assertTrue(result['app_main_budget_pass']['5000'])

    def test_later_session_is_not_initial_session(self):
        log = event('world_initial_playable_candidate', resident_tiles=2, ray_guard_complete=0)
        log += '\n' + event(session=1, elapsed_ms=7500)
        self.assertEqual(inspect(log)['startup_budget_disposition'], 'unqualified')

    def test_activation_absent_disabled_and_requested_missing_are_distinct(self):
        self.assertEqual(readiness_activation('')['status'], 'not_observed')
        self.assertEqual(readiness_activation('', False)['status'], 'disabled')
        self.assertEqual(readiness_activation(ACTIVATION, True)['status'], 'enabled')
        for log, requested in [('', True), (ACTIVATION, False), (ACTIVATION.replace('enabled=1', 'enabled=0'), None)]:
            with self.subTest(log=log, requested=requested), self.assertRaises(ValueError):
                readiness_activation(log, requested)
        with self.assertRaises(ValueError):
            initial_playable_report(event())

    def test_damage_clock_scope_and_missing_predicates_rejected(self):
        cases = [event(schema=2), event(clock_origin='sdl_uptime'), event(elapsed_ms='nan'),
                 event(visible_missing=1), event(collision_ready=0), event(requested_ready=0),
                 event(ray_ready=0), event(ray_guard_complete=0), event(resident_tiles=2),
                 event(requested_tiles=18), event(scope='visible_requested_region'), event(ray_required=2),
                 event(requested_set_hash=1 << 64), event(fov=0), event().replace(' collision_ready=1', ''),
                 event() + ' elapsed_ms=10', 'other ' + event(), event() + ' ' + event(),
                 event() + '\n' + event(),
                 event('world_initial_playable_candidate', elapsed_ms=6000) + '\n' + event()]
        for text in cases:
            with self.subTest(text=text), self.assertRaises(ValueError):
                inspect(text)

if __name__ == '__main__':
    unittest.main()
