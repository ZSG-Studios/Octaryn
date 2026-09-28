import csv
from pathlib import Path
import tempfile
import unittest

from authority_item_listener import summarize


class AuthorityItemProfileTest(unittest.TestCase):
    def fixture(self, directory, missing=False):
        path = Path(directory) / "loop.csv"
        fields = ["schema", "pump", "authority_tick", "control_wall_ms", "network_wall_ms",
                  "entity_wall_ms", "authority_session_wall_ms", "total_wall_ms", "process_alloc_bytes"]
        with path.open("w", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(fields)
            for index in range(1, 4001):
                if missing and index == 3500:
                    continue
                active = index % 10 == 0
                writer.writerow([1, index, index // 10, 0, 0, 0, 4 if active else .01,
                                 4 if active else .01, 100 if active else 0])
        return path

    def test_idle_pumps_do_not_hide_simulation(self):
        with tempfile.TemporaryDirectory() as directory:
            report = summarize(self.fixture(directory))
        self.assertEqual(report["all_pumps_ms"]["median"], .01)
        self.assertEqual(report["advancing_authority_pumps_ms"]["median"], 4)
        self.assertEqual(report["advancing_authority_pumps_ms"]["samples"], 100)

    def test_missing_profile_sample_rejects_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "dropped pump"):
                summarize(self.fixture(directory, missing=True))


if __name__ == "__main__":
    unittest.main()
