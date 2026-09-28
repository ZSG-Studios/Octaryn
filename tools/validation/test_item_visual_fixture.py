import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from item_visual_fixture import validate_completion, pose_evidence, wait_for_owner_stop


class CompletionTests(unittest.TestCase):
    def evidence(self, **changes):
        proof = dict(status="passed", count=4, awake=0, settledBeforeMeasurement=4, finalAwake=0,
                     quantityConserved=True)
        proof.update(changes)
        return "authority_item_listener=" + json.dumps(proof)

    def test_sleeping_and_awake_authority_evidence(self):
        self.assertEqual(validate_completion(self.evidence(), 4, 0)["finalAwake"], 0)
        self.assertEqual(validate_completion(self.evidence(awake=4, finalAwake=4), 4, 4)["count"], 4)

    def test_incomplete_or_changed_workload_is_rejected(self):
        for changes in [dict(status="failed"), dict(count=3), dict(awake=1),
                        dict(settledBeforeMeasurement=3), dict(finalAwake=1)]:
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                validate_completion(self.evidence(**changes), 4, 0)

    def test_missing_and_duplicate_completion_is_rejected(self):
        for log in ["", self.evidence() + "\n" + self.evidence()]:
            with self.assertRaises(ValueError):
                validate_completion(log, 4, 0)

    def test_pose_changes_reject_sleeping_but_not_moving(self):
        initial = [dict(EntityId=i, Generation=i, ItemId=i, Count=1, Y=.2) for i in range(1, 5)]
        final = [dict(p) for p in initial]
        def log():
            return '\n'.join('authority_item_poses=' + json.dumps(dict(phase=phase, poses=poses))
                             for phase, poses in [('initial', initial), ('final', final)])
        self.assertTrue(pose_evidence(log(), 4, 0)['stable'])
        final[0]['Y'] += .1
        with self.assertRaises(ValueError):
            pose_evidence(log(), 4, 0)
        self.assertFalse(pose_evidence(log(), 4, 4)['stable'])
        final[0]['Count'] = 2
        with self.assertRaises(ValueError):
            pose_evidence(log(), 4, 4)

    def test_owner_stop_reaches_child_without_termination(self):
        with tempfile.TemporaryDirectory() as directory:
            request = Path(directory) / 'stop-request'
            with subprocess.Popen([sys.executable, '-c',
                                   'import sys; sys.exit(0 if input() == "stop" else 4)'],
                                  stdin=subprocess.PIPE, text=True) as child:
                request.touch()
                self.assertEqual(wait_for_owner_stop(child, request, 600), 0)


if __name__ == "__main__":
    unittest.main()
