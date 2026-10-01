import csv
from pathlib import Path
import tempfile
import unittest

from authority_execution_report import summarize


class AuthorityExecutionReportTests(unittest.TestCase):
    def fixture(self, path):
        common = dict(schema=2, thread_cycles=100, thread_cycles_valid=1,
                      gc_pause_ms=0, process_alloc_bytes=0)
        loops = [dict(common, pump=i + 1, authority_tick=tick, elapsed_wall_ms=elapsed,
                      total_wall_ms=1) for i, (tick, elapsed) in enumerate(
                          [(0, 0), (1, 17), (1, 18), (2, 34), (4, 90)])]
        modules = [dict(common, frame=i, whole_tick_ms=i + 1) for i in range(4)]
        authority = [dict(frame=i, schedule_wall_ms=i + 1, command_thread_cycles=100) for i in range(4)]
        for name, rows in [("loop.csv", loops), ("module.csv", modules), ("authority.csv", authority)]:
            with (path / name).open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(rows)

    def test_service_gap_and_exact_module_phase_join(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            self.fixture(path)
            report = summarize(path, warmup=0)
            self.assertEqual(report["advancing_pumps"]["wall_ms"]["samples"], 3)
            self.assertEqual(report["authority_service"]["completion_gap_ms"]["worst"], 56)
            self.assertEqual(report["authority_service"]["pumps_with_catchup"], 1)
            self.assertEqual(report["authority_service"]["profiled_owner_work_per_completion_ms"]["worst"], 2)
            self.assertEqual(report["authority_service"]["leading_partial_excluded"]["pump_count"], 2)
            worst = report["module_ticks"]["worst"][0]
            self.assertEqual(worst["frame"], 3)
            self.assertEqual(worst["authority_phases"]["schedule_wall_ms"], 4)
            self.assertEqual(worst["thread_cycles"], 100)

    def test_missing_record_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            self.fixture(path)
            target = path / "loop.csv"
            lines = target.read_text().splitlines()
            target.write_text("\n".join(lines[:2] + lines[3:]) + "\n")
            with self.assertRaisesRegex(ValueError, "Missing listener"):
                summarize(path, warmup=0)

    def test_unavailable_cycles_are_not_zero_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            self.fixture(path)
            for name in ("loop.csv", "module.csv"):
                target = path / name
                target.write_text(target.read_text().replace("2,100,1,0,0", "2,0,0,0,0"))
            report = summarize(path, warmup=0)
            self.assertIsNone(report["module_ticks"]["thread_cycles"])
            self.assertIsNone(report["module_ticks"]["worst"][0]["thread_cycles"])

    def test_trailing_poll_cost_is_not_a_complete_tick(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            self.fixture(path)
            target = path / "loop.csv"
            with target.open("a") as stream:
                stream.write("2,100,1,0,0,6,4,100,50\n")
            report = summarize(path, warmup=0)
            self.assertEqual(report["authority_service"]["profiled_owner_work_per_completion_ms"]["worst"], 2)
            self.assertEqual(report["authority_service"]["trailing_unclosed_excluded"],
                             dict(pump_count=1, profiled_owner_work_ms=50))


if __name__ == "__main__":
    unittest.main()
