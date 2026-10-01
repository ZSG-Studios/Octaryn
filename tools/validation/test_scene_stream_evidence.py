"""Fault injection for dense scene publication, budget and fence evidence."""
import json
from pathlib import Path
import tempfile
import unittest

from scene_stream_evidence import BUDGET, continuity
from scene_stream_fixture import generate


def line(marker, **fields):
    return marker + " " + " ".join(f"{key}={value}" for key, value in fields.items()) + "\n"


def valid_log():
    result = ""
    for frame in range(10, 18):
        result += line("scene_continuity", frame=frame, generation=1 if frame < 13 else 2,
            resident=5, pending=int(frame in (11, 12)), cpu_pending=0,
            pending_roots=int(frame == 12), pending_rays=0, ray_requested=1, ray_enabled=1,
            coverage_complete=1, reflection_valid=1, ray_generation=1,
            reservation_bytes=8*1024**2, allocated_reservation_bytes=6*1024**2,
            retired_bytes=1024**2 if frame == 14 else 0, budget=BUDGET,
            retired=4 if frame == 14 else 0, fence_signal=frame, fence_completed=frame-1)
    for part in range(4, 8):
        result += line("scene_part_published", frame=13, part=part, instances=1,
                       roots_ready=1, rays_ready=1, generation=2)
    for part in range(4):
        result += line("scene_part_retired", frame=14, part=part, fence_signal=13, published=1)
        result += line("scene_part_collected", frame=15, part=part, fence_signal=13,
                       fence_completed=14, ray_idle=1, upload_idle=1)
    return result


class SceneStreamEvidenceTests(unittest.TestCase):
    def test_complete_dense_interval(self):
        evidence = continuity(valid_log(), 10, 17)
        self.assertEqual(evidence["checked_frames"], 8)
        self.assertEqual(evidence["incoming_parts"], [4, 5, 6, 7])
        self.assertEqual(evidence["retired_parts"], [0, 1, 2, 3])

    def test_pending_queued_work_is_not_an_allocation(self):
        text = valid_log().replace("reservation_bytes=8388608", f"reservation_bytes={BUDGET*2}")
        self.assertEqual(continuity(text, 10, 17)["maximum_desired_plan_bytes"], BUDGET*2)

    def test_missing_and_duplicate_frames_fail(self):
        text = valid_log()
        first = text.splitlines(keepends=True)[0]
        for broken in (text.replace(first, ""), first + text):
            with self.assertRaisesRegex(RuntimeError, "frames"):
                continuity(broken, 10, 17)

    def test_ready_reflection_dropout_fails(self):
        for field in ("ray_enabled", "reflection_valid", "coverage_complete", "ray_requested", "ray_generation"):
            with self.subTest(field=field), self.assertRaises(RuntimeError):
                continuity(valid_log().replace(field + "=1", field + "=0", 1), 10, 17)

    def test_no_pending_work_is_not_a_streaming_proof(self):
        for text in (valid_log().replace("pending=1", "pending=0"),
                     valid_log().replace("pending_roots=0", "pending_roots=1"),
                     valid_log().replace("pending_rays=0", "pending_rays=1")):
            with self.assertRaises(RuntimeError):
                continuity(text, 10, 17)

    def test_early_publication_or_missing_material_part_fails(self):
        for text in (valid_log().replace("roots_ready=1", "roots_ready=0", 1),
                     valid_log().replace("rays_ready=1", "rays_ready=0", 1),
                     valid_log().replace("frame=13 part=7", "frame=13 part=8")):
            with self.assertRaises(RuntimeError):
                continuity(text, 10, 17)

    def test_retained_allocations_cannot_raise_the_budget(self):
        for text in (valid_log().replace(f"budget={BUDGET}", f"budget={BUDGET*2}"),
                     valid_log().replace("allocated_reservation_bytes=6291456", f"allocated_reservation_bytes={BUDGET}")):
            with self.assertRaisesRegex(RuntimeError, "budget"):
                continuity(text, 10, 17)

    def test_collection_requires_all_actual_owner_fences(self):
        for a, b in (("fence_completed=14 ray_idle=1", "fence_completed=12 ray_idle=1"),
                     ("ray_idle=1", "ray_idle=0"), ("upload_idle=1", "upload_idle=0"),
                     ("frame=15 part=0", "frame=12 part=0")):
            with self.subTest(field=a), self.assertRaisesRegex(RuntimeError, "fences"):
                continuity(valid_log().replace(a, b, 1), 10, 17)

    def test_fixture_keeps_every_station_and_uses_authority_input(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            generate(root)
            source = json.loads((root / "reflection.gltf").read_text())
            self.assertEqual([node["mesh"] for node in source["nodes"]], [0, 1, 2])
            self.assertEqual(source["nodes"][1]["translation"], [260, 0, 0])
            self.assertEqual([len(mesh["primitives"]) for mesh in source["meshes"]], [4, 4, 1])
            triangles = sum(source["accessors"][part["indices"]]["count"] // 3
                            for mesh in source["meshes"] for part in mesh["primitives"])
            self.assertEqual(triangles, 18)
            route = json.loads((root / "route.json").read_text())
            self.assertEqual(sum(phase["seconds"] for phase in route["phases"]), 60)
            self.assertEqual(route["phases"][2]["target"], [260, 6])
            self.assertEqual(route["phases"][2]["tolerance"], .75)
            self.assertFalse(route["phases"][2].get("sprint", False))
            self.assertTrue(route["phases"][1]["sprint"])


if __name__ == "__main__":
    unittest.main()
