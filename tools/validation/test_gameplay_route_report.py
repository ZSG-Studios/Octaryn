import csv
from pathlib import Path
import tempfile
import unittest

from gameplay_route_report import summarize_gameplay_route


class GameplayRouteReportTest(unittest.TestCase):
    route = {"version": 1, "phases": [{"name": "walk", "seconds": 2, "forward": 1, "min_distance": 1}]}

    def fixture(self, directory, stationary=False, unchanged_tick=False):
        path = Path(directory)
        rows = []
        for index in range(3):
            rows.append(dict(frame=index, seconds=index, phase="walk", phase_index=0,
                phase_seconds=index, authority_tick=0 if unchanged_tick else index,
                ack=index, sent_input=index, authority_x=0, authority_y=2,
                authority_z=0 if stationary else index, authority_yaw=0, authority_pitch=0,
                authority_distance=0 if stationary else index, phase_distance=0 if stationary else index,
                forward=1, strafe=0, sprint=0, command_yaw=0, command_pitch=0, target_error=-1,
                blocked=int(stationary), pending=0))
        with (path / "gameplay-route.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)
        (path / "client.log").write_text("gameplay_route_phase name=walk completed=1 passed=1\n"
                                        "gameplay_route_result passed=1 complete=1 authority_distance=2\n")
        return path

    def test_authority_positions_establish_distance(self):
        with tempfile.TemporaryDirectory() as directory:
            result = summarize_gameplay_route(self.fixture(directory), self.route)
        self.assertEqual(result["authority_distance_metres"], 2)
        self.assertTrue(result["traversal_observed"])

    def test_stationary_does_not_pass_from_log_claim(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "distance/waypoint"):
                summarize_gameplay_route(self.fixture(directory, stationary=True), self.route)

    def test_pose_movement_without_authority_tick_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "without an authority tick"):
                summarize_gameplay_route(self.fixture(directory, unchanged_tick=True), self.route)


if __name__ == "__main__":
    unittest.main()
