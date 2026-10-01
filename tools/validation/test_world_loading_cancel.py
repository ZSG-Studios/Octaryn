"""Reject cancellation evidence that hides content work, leaked authority or unjoined jobs."""
from pathlib import Path
import struct
import tempfile
import unittest

from validate_world_loading_cancel import cancellation_evidence
from world_library_evidence import IO_FIELDS, SHOWN


class LoadingCancellationEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        (self.folder / "loading").mkdir()
        header = bytearray(54)
        header[:2] = b"BM"
        struct.pack_into("<ii", header, 18, 1280, 720)
        for name in ("Checking-the-selected-world---", "Starting-world", "Preparing-geometry", "Waiting-for-player"):
            (self.folder / "loading" / (name + ".bmp")).write_bytes(header)
        (self.folder / "frame.bmp").write_bytes(header)
        self.counts = " ".join(field + "=0" for field in IO_FIELDS)
        self.menu = f"world_library_io phase=menu {self.counts}\n{SHOWN}\n"
        self.clean = ("world_library_surface visible=1 loading=0\n" +
                      "capture_clean_exit=passed active_owned_processes=0 before_safety_cleanup=1\n")

    def queued(self):
        return (self.menu + "world_loading_validation_cancel stage=Checking the selected world... occurrence=1\n" +
                f"world_library_io phase=exit {self.counts}\n" + self.clean)

    def geometry(self):
        return (self.menu + "world_loading_validation_cancel stage=Preparing geometry occurrence=1\n" +
                "map_boot responsiveness=1 worker_jobs=1 result=cancelled\nworld_load_return_menu canceled=1\n" +
                "world_loading_cleanup server_running=0 result=cancelled\n" + self.clean)

    def test_queued_zero_io_proof(self):
        self.assertEqual(cancellation_evidence(self.queued(), self.folder,
                         "Checking the selected world...", 1)["unselected_content_reads"], 0)

    def test_post_cancel_source_read_is_rejected(self):
        text = self.queued().replace(f"phase=exit {self.counts}", f"phase=exit {self.counts.replace('model_loads=0', 'model_loads=1')}")
        with self.assertRaises(AssertionError):
            cancellation_evidence(text, self.folder, "Checking the selected world...", 1)

    def test_native_canceled_worker_is_required(self):
        cancellation_evidence(self.geometry(), self.folder, "Preparing geometry", 1)
        with self.assertRaises(AssertionError):
            cancellation_evidence(self.geometry().replace("result=cancelled", "result=ready"), self.folder,
                                  "Preparing geometry", 1)

    def test_safety_cleanup_does_not_prove_clean_exit(self):
        with self.assertRaises(AssertionError):
            cancellation_evidence(self.geometry().replace(self.clean, ""), self.folder, "Preparing geometry", 1)

    def test_action_must_run_once(self):
        with self.assertRaises(AssertionError):
            cancellation_evidence(self.geometry() + "world_loading_validation_cancel stage=Preparing geometry occurrence=1\n",
                                  self.folder, "Preparing geometry", 1)

    def test_cancel_ack_overlay_is_not_library_return(self):
        with self.assertRaises(AssertionError):
            cancellation_evidence(self.geometry().replace("visible=1 loading=0", "visible=0 loading=1"),
                                  self.folder, "Preparing geometry", 1)

    def test_cancel_must_not_enter_game(self):
        with self.assertRaises(AssertionError):
            cancellation_evidence(self.geometry() + "world_scene_selected reload=1", self.folder, "Preparing geometry", 1)

    def test_actual_player_wait_requires_its_own_completion(self):
        text = (self.menu + "map_boot responsiveness=1 worker_jobs=1 result=ready\nworld_scene_selected reload=1\n" +
                "world_loading_validation_cancel stage=Waiting for player occurrence=2\n" +
                "world_loading_cancel phase=player_wait\n" + self.clean)
        cancellation_evidence(text, self.folder, "Waiting for player", 2)
        with self.assertRaises(AssertionError):
            cancellation_evidence(text.replace("world_loading_cancel phase=player_wait", ""), self.folder, "Waiting for player", 2)


if __name__ == "__main__":
    unittest.main()
