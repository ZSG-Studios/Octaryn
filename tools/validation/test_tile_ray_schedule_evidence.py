"""Keep actual AS work separate from free lifecycle polls and CPU target claims."""
import unittest
from tile_performance_report import parse_tile_work
from capture_tile_world import ray_policy_evidence


class RayScheduleEvidenceTests(unittest.TestCase):
    def record(self, **changes):
        fields = dict(frame=1,polls=4,wait_fences=2,wait_allocations=1,operations=1,submissions=1,
                      inflight=4,capacity=4,published=0,cancelled=0,work_tile=2,work_step=4,
                      work_ms=.15,cpu_ms=.35,soft_budget_ms=.2,total_polls=4,total_wait_fences=2,
                      total_wait_allocations=1,total_operations=1,total_submissions=1)
        fields.update(changes)
        return 'tile_ray_schedule ' + ' '.join(f'{key}={value}' for key,value in fields.items())

    def test_real_work_and_soft_overrun_remain_visible(self):
        result = parse_tile_work(self.record())['stages']['before_authority_ready']
        self.assertEqual(result['ray_schedule_submissions']['median'],1)
        self.assertAlmostEqual(result['ray_schedule_soft_budget_overrun_ms']['worst'],.15)
        self.assertEqual(result['tile_ray_schedule_cpu_ms']['worst'],.35)
        allocation = parse_tile_work(self.record(work_step=2,submissions=0,total_submissions=0))
        self.assertEqual(allocation['stages']['before_authority_ready']['ray_schedule_submissions']['worst'],0)

    def test_limit_and_classification_corruption_rejected(self):
        for changes in (dict(polls=5),dict(inflight=5),dict(capacity=5),dict(operations=2),
                        dict(wait_fences=4),dict(submissions=2),dict(work_step=2),dict(work_step=1),
                        dict(operations=0,submissions=0),dict(work_ms='nan')):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                parse_tile_work(self.record(**changes))

    def test_wait_only_is_not_submission(self):
        result = parse_tile_work(self.record(operations=0,submissions=0,work_tile=4294967295,
                                 work_step=6,work_ms=0,total_operations=0,total_submissions=0))
        self.assertEqual(result['stages']['before_authority_ready']['ray_schedule_operations']['worst'],0)

    def test_explicit_serial_control_activation(self):
        prefix='tile_ray_policy capacity=1 max_operations=1 soft_budget_ms=0.200\n'
        log=prefix+self.record(capacity=1,inflight=1,polls=1,wait_fences=0,wait_allocations=0)
        self.assertIn('not the previous algorithm',ray_policy_evidence(log,1)['control'])
        parse_tile_work(log)
        for text,request in ((log,4),(log.replace('max_operations=1','max_operations=2'),1),
                              (log.replace('inflight=1','inflight=2'),1),(self.record(),4)):
            with self.subTest(text=text), self.assertRaises(ValueError):ray_policy_evidence(text,request)


if __name__ == '__main__':
    unittest.main()
