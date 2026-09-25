"""Near admission evidence must match native fenced phase and clean-sweep policy."""
import unittest
from test_world_gi_capture import fixture
from world_gi_capture import assert_world_gi, assert_world_gi_sequence


def ready(frame=240, sweeps=4, failures=4, epoch=7, start=200):
    value=fixture()
    value.update(render_frame=frame,block_transport_statistics_frame=frame,
        block_transport_admission_sweeps=sweeps,block_transport_measured_admission_sweeps=sweeps,
        block_transport_epoch=epoch,block_transport_contributor_start_frame=start)
    value['block_transport_counters'][11]=failures
    return value


class WorldGiCaptureEdgeTests(unittest.TestCase):
    def test_first_contributor_submission_can_be_ready_after_its_fence(self):
        value=ready(start=240)
        self.assertIs(assert_world_gi(value),value)
        value['block_transport_statistics_frame']=239
        with self.assertRaises(RuntimeError):
            assert_world_gi(value)

    def test_unstarted_uint64_sentinel_never_qualifies(self):
        value=ready(frame=2**64-1,start=2**64-1)
        with self.assertRaises(RuntimeError):
            assert_world_gi(value)

    def test_high_frame_bits_are_preserved(self):
        value=ready(frame=2**32+240,start=2**32+200)
        self.assertIs(assert_world_gi(value),value)
        value['block_transport_statistics_frame']=240
        with self.assertRaises(RuntimeError):
            assert_world_gi(value)

    def test_fence_sweep_and_occupancy_types_are_strict(self):
        fields=('render_frame','block_transport_statistics_frame','block_transport_admission_sweeps',
            'block_transport_measured_admission_sweeps','block_transport_contributor_start_frame',
            'block_transport_pinned_rows','block_transport_ready_rows','block_transport_selection_occupied')
        for field in fields:
            for replacement in (False,True,1.0,None,float('nan'),-1,2**64):
                value=ready();value[field]=replacement
                with self.subTest(field=field,value=replacement),self.assertRaises(RuntimeError):
                    assert_world_gi(value)

    def test_gpu_counters_are_unsigned32_not_bool_or_unbounded(self):
        for index in range(12):
            for replacement in (True,-1,1.0,2**32):
                value=ready();value['block_transport_counters'][index]=replacement
                with self.subTest(index=index,value=replacement),self.assertRaises(RuntimeError):
                    assert_world_gi(value)

    def test_every_occupied_row_must_be_initialized_ready_and_selected(self):
        for field in ('block_transport_ready_rows','block_transport_selection_occupied'):
            for replacement in (15,17):
                value=ready();value[field]=replacement
                with self.subTest(field=field,value=replacement),self.assertRaises(RuntimeError):
                    assert_world_gi(value)
        for replacement in (15,17):
            value=ready();value['block_transport_counters'][9]=replacement
            with self.subTest(initialized=replacement),self.assertRaises(RuntimeError):
                assert_world_gi(value)
        value=ready()
        value.update(block_transport_pinned_rows=65536,block_transport_ready_rows=65536,
            block_transport_selection_occupied=65536)
        value['block_transport_counters'][8:10]=[65536,65536]
        self.assertIs(assert_world_gi(value),value)
        value['block_transport_counters'][8:10]=[65537,65537]
        with self.assertRaises(RuntimeError):
            assert_world_gi(value)

    def test_resource_counts_are_strict_even_for_partial_evidence(self):
        for field in ('block_transport_gpu_bytes','block_transport_total_gpu_bytes'):
            for replacement in (True,1.0,None,-1,2**64):
                value=ready();value[field]=replacement
                with self.subTest(field=field,value=replacement),self.assertRaises(RuntimeError):
                    assert_world_gi(value,require_ready=False)

    def test_partial_evidence_does_not_claim_complete_admission(self):
        value=ready()
        value.update(block_transport_ready=False,block_transport_admission_clean=False,
            block_transport_contributor_start_frame=2**64-1,block_transport_admission_sweeps=0)
        self.assertIs(assert_world_gi(value,require_ready=False),value)
        with self.assertRaises(RuntimeError):
            assert_world_gi(value)

    def test_transient_error_needs_failure_boundary_then_complete_clean_sweep(self):
        first=ready()
        for swept in (4,5):
            later=ready(frame=300,sweeps=swept,failures=5)
            with self.subTest(swept=swept),self.assertRaisesRegex(RuntimeError,'complete clean sweep'):
                assert_world_gi_sequence([first,later])
        later=ready(frame=300,sweeps=6,failures=5)
        self.assertEqual(assert_world_gi_sequence([first,later]),[first,later])
        self.assertEqual(assert_world_gi_sequence([first,ready(frame=241)]),[first,ready(frame=241)])

    def test_failure_counter_wrap_still_needs_a_new_clean_sweep(self):
        first=ready(failures=2**32-1)
        with self.assertRaisesRegex(RuntimeError,'complete clean sweep'):
            assert_world_gi_sequence([first,ready(frame=300,sweeps=5,failures=0)])
        assert_world_gi_sequence([first,ready(frame=300,sweeps=6,failures=0)])

    def test_sequence_fence_sweep_and_first_phase_cannot_regress(self):
        for later in (ready(frame=240),ready(frame=239),ready(frame=300,sweeps=3),ready(frame=300,start=201)):
            with self.subTest(later=later),self.assertRaises(RuntimeError):
                assert_world_gi_sequence([ready(),later])

    def test_geometry_reset_has_its_own_phase_and_clean_sweep(self):
        later=ready(frame=300,sweeps=1,failures=0,epoch=8,start=300)
        assert_world_gi_sequence([ready(),later])

    def test_sequence_epoch_must_be_explicit_and_valid(self):
        for epoch in (None,False,True,0,1.0,2**32-1):
            with self.subTest(epoch=epoch),self.assertRaises(RuntimeError):
                assert_world_gi_sequence([ready(epoch=epoch)])


if __name__ == '__main__':
    unittest.main()
