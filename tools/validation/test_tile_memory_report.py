import csv
from pathlib import Path
import tempfile
import unittest

from tile_memory_report import cycle_memory


class TileMemoryReport(unittest.TestCase):
    def report(self, count, growth):
        with tempfile.TemporaryDirectory() as directory:
            case = Path(directory)
            with (case/'frame-timing.csv').open('w', newline='') as output, \
                 (case/'tile-route.csv').open('w', newline='') as motion:
                writer, route = csv.writer(output), csv.writer(motion)
                writer.writerow(['frame','schema_version','process_resident_bytes',
                                 'allocated_gpu_estimate_bytes','gpu_local_usage_bytes'])
                route.writerow(['frame','ready_frame','cycle','phase'])
                for cycle in range(count):
                    for offset in range(720,960):
                        frame = 180 + cycle*960 + offset
                        writer.writerow([frame,2,500*1024**2+cycle*growth,100*1024**2,110*1024**2])
                        route.writerow([frame,frame,cycle,'origin'])
            return cycle_memory(case)

    def test_short_run_cannot_qualify_plateau(self):
        self.assertFalse(self.report(7,0)['qualified'])

    def test_stable_long_run_passes(self):
        self.assertTrue(self.report(8,0)['passed'])

    def test_growth_fails_even_below_range_limit(self):
        result = self.report(8,3*1024**2)
        self.assertTrue(result['qualified'])
        self.assertFalse(result['passed'])


if __name__ == '__main__':
    unittest.main()
