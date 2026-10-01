"""Keep tiled codec comparisons on the exact same native view and sample phase."""
import copy
from pathlib import Path
import tempfile
import unittest

from tile_quality_evidence import quality_signature


class TileQualityTests(unittest.TestCase):
    def test_matched_native_and_rejection_gates(self):
        with tempfile.TemporaryDirectory() as directory:
            case = Path(directory)
            (case / 'camera-motion.csv').write_text(
                'frame,ready_frame,phase,eye_x,eye_y,eye_z,yaw,pitch\n3032,3000,static,0,3,-20,.6,-.25\n')
            capture = dict(width=2560, height=1440,
                lighting=dict(render_frame=3032, ray_enabled=True, sky_time=[0,1,1,0], sky_light_direction=[0,-1,0,1],
                              shadow_distance=1024, reflection_distance=1024, lighting_debug_view=0,
                              local_light_count=0, gi_mode='direct', ray_coverage_complete=True,
                              ray_pending_columns=0, ray_active_jobs=0),
                observation=dict(frame=3032, fixed_sampling=True, delta_ms=1000/60, sampling_frame=3000,
                                 reflection_sampling_frame=3000, jitter=[0,0]),
                residency=dict(resident=299, wanted=299, preparing=0, uploading=0))
            rows = {3032: dict(render_width=2560, render_height=1440, upscaler_mode=0, dynamic_resolution=0)}
            log = ('map_validation_sampling fixed=1 index=ready_frame\n'
                   'world_capture frame=3032 nonclear_pixels=3686400 eye=0,3,-20 yaw=.6 pitch=-.25 fov=1.570796 path=frame.bmp\n')
            def check(value, records=rows, ready=3000, text=log):
                return quality_signature(case, dict(capture=value), records, ready, [0,3,-20,.6,-.25],299,
                                         text)
            self.assertEqual(check(capture)['full_resident_tiles'], 299)
            for group, key, value in [('residency','resident',298), ('residency','uploading',1),
                                     ('observation','sampling_frame',2999), ('observation','delta_ms',20),
                                     ('observation','reflection_sampling_frame',2999), ('observation','fixed_sampling',False),
                                     ('lighting','sky_time',[0,1,1,10]), ('observation','frame',3031),
                                     ('lighting','ray_coverage_complete',False), ('lighting','ray_active_jobs',1),
                                     ('lighting','shadow_distance',None)]:
                broken=copy.deepcopy(capture);broken[group][key]=value
                with self.subTest(group=group,key=key), self.assertRaises(ValueError):
                    check(broken)
            with self.assertRaisesRegex(ValueError,'exact common'):
                check(capture,ready=3001)
            with self.assertRaisesRegex(ValueError,'internal dimensions'):
                check(capture,{3032:dict(rows[3032],render_width=1920)})
            with self.assertRaisesRegex(ValueError,'frame join'):
                check(capture,{3031:rows[3032]})
            with self.assertRaisesRegex(ValueError,'camera/FOV'):
                check(capture,text=log.replace('world_capture frame=3032','world_capture frame=3031'))
            for name in ('shadow_distance', 'reflection_distance', 'lighting_debug_view', 'local_light_count'):
                changed=copy.deepcopy(capture);changed['lighting'][name] += 1
                with self.subTest(setting=name):
                    self.assertNotEqual(check(capture),check(changed))
            self.assertNotEqual(check(capture),check(capture,text=log.replace('fov=1.570796','fov=1.2')))


if __name__ == '__main__':
    unittest.main()
