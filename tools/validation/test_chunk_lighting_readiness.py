"""World transport readiness must remain separate from geometry/ray readiness."""
import csv
from pathlib import Path
import tempfile
import unittest

from benchmark_chunk_loading import summarize


class LightingReadiness(unittest.TestCase):
    def case(self, fields, values):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name)
        (path / 'client.log').write_text('')
        with (path / 'frame-timing.csv').open('w', newline='') as output:
            writer = csv.writer(output)
            writer.writerow(('frame', 'total_ms', 'columns', 'pending_meshes', 'ray_pending', *fields))
            writer.writerows(values)
        return path

    def test_geometry_ready_does_not_imply_lighting(self):
        path = self.case(('gi_ready',), [
            (1, 33, 81, 0, 0, 0),
            (2, 33, 81, 0, 0, 0),
            (3, 33, 81, 0, 0, 1),
        ])
        result = summarize(path, 4)
        self.assertEqual(result['milestones']['full_columns_meshes_and_rays']['frame'], 1)
        self.assertEqual(result['milestones']['full_columns_meshes_rays_and_lighting']['frame'], 3)
        self.assertTrue(result['last_gi_ready'])

    def test_old_evidence_has_no_invented_light_milestone(self):
        path = self.case((), [(1, 33, 81, 0, 0)])
        result = summarize(path, 4)
        self.assertIn('full_columns_meshes_and_rays', result['milestones'])
        self.assertNotIn('full_columns_meshes_rays_and_lighting', result['milestones'])
        self.assertNotIn('last_gi_ready', result)

    def test_missing_columns_cannot_pass_with_zero_backlog(self):
        path = self.case(('gi_ready',), [
            (1, 33, 80, 0, 0, 1),
        ])
        self.assertNotIn('full_columns_meshes_rays_and_lighting', summarize(path, 4)['milestones'])


if __name__ == '__main__':
    unittest.main()
