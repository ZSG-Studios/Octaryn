"""Reject false workload matches and retain tails in matrix comparisons."""
import json
from pathlib import Path
import tempfile
import unittest

from summarize_performance_matrix import (aggregate, camera_records, capture_lighting,
    matched_frames, read_case, require_same_camera, summarize_group)
from performance_summary import distribution


class MatrixEvidence(unittest.TestCase):
    def test_camera_matches_by_ready_frame_not_submission_offset(self):
        first = {0: (100, ('warmup', 0., 2., -20., .6, -.25))}
        second = {0: (250, ('warmup', 0., 2., -20., .6, -.25))}
        require_same_camera([first, second])
        second[0] = (250, ('warmup', 0., 2.02, -20., .6, -.25))
        with self.assertRaisesRegex(ValueError, 'coordinates'):
            require_same_camera([first, second])

    def test_camera_rejects_nonfinite_and_missing_ready_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'camera-motion.csv'
            for row in ('1,1,warmup,0,2,-20,0,0', '1,0,warmup,nan,2,-20,0,0'):
                path.write_text('frame,ready_frame,phase,eye_x,eye_y,eye_z,yaw,pitch\n' + row + '\n')
                with self.assertRaises(ValueError): camera_records(path)

    def test_captured_sun_mismatch_rejected_even_when_metadata_matches(self):
        cases = []
        camera = {0: (100, ('warmup', 0., 2., -20., .6, -.25))}
        for variant in ('reference', 'adaptive'):
            for repeat in range(3):
                sun = [0,-1,0,1] if variant == 'reference' else [.02,-1,0,1]
                cases.append(dict(identity=['identical metadata'], camera=camera,result=dict(map={}),
                    entry=dict(variant=variant, repeat=repeat), sunlight={360:(sun,[0,1,1,0])} if repeat==0 else {}))
        with self.assertRaisesRegex(ValueError, 'sun direction'):
            summarize_group(cases)

    def test_capture_frame_requires_actual_camera_record(self):
        with tempfile.TemporaryDirectory() as directory:
            case=Path(directory)
            (case/'frame.bmp.lighting.json').write_text(json.dumps(dict(render_frame=200,
                sky_light_direction=[0,-1,0,1], sky_time=[0,1,1,0])))
            with self.assertRaisesRegex(ValueError, 'no observed camera'):
                capture_lighting(case,{0:(100,('static',0,0,0,0,0))})

    def test_diagnostic_timings_are_not_qualified_by_status(self):
        with tempfile.TemporaryDirectory() as directory:
            case=Path(directory)
            (case/'result.json').write_text(json.dumps(dict(status='captured', timing_qualification=True,
                                                           ray_diagnostics=True)))
            with self.assertRaisesRegex(ValueError, 'Instrumented'):
                read_case(dict(case=str(case)))

    def test_union_capture_exclusions_align_all_runs(self):
        with tempfile.TemporaryDirectory() as directory:
            cases=[]
            for index,offset in enumerate((100,300)):
                path=Path(directory)/str(index);path.mkdir()
                if index==0:
                    (path/'frame.bmp.observation.json').write_text(json.dumps(dict(frame=offset+140)))
                camera={ready:(ready+offset,('static',0,2,-20,.6,-.25)) for ready in range(200)}
                profile={frame:{} for frame,_ in camera.values()}
                cases.append(dict(camera=camera,profiles=dict(cpu=profile,gpu=profile,lighting=profile),
                    result=dict(warmup_frames=10),entry=dict(case=str(path))))
            selected,excluded,warmup=matched_frames(cases)
            self.assertEqual(excluded,[139,140,141,142])
            self.assertEqual(len(selected),186)
            self.assertEqual(warmup,10)

    def test_unlabeled_history_search_cannot_enter_adaptive_comparison(self):
        with tempfile.TemporaryDirectory() as directory:
            case=Path(directory)
            (case/'result.json').write_text(json.dumps(dict(status='measured', timing_qualification=True,
                uncapped_fps=True, fixed_lighting=True, rt_reference=False, rt_sparse=False,
                rt_history_search=True, draw_mode='direct', lod_pixels=0)))
            with self.assertRaisesRegex(ValueError, 'variant does not match'):
                read_case(dict(case=str(case), variant='adaptive'))

    def test_aggregate_worst_is_global_maximum(self):
        runs=[]
        for values in ([1,2,3],[1,2,100],[1,2,4]):
            metric=distribution(values)
            runs.append({group:{'test_ms':metric} for group in ('cpu','gpu','lighting','memory')})
        result=aggregate(runs)['gpu']['test_ms']
        self.assertEqual(result['worst'],100)
        self.assertEqual(result['median'],2)


if __name__=='__main__':unittest.main()
