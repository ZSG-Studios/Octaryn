"""Fault-inject menu/loading evidence so a missing or unsafe handoff cannot pass."""
from pathlib import Path
import struct
import tempfile
import unittest

from world_library_evidence import IO_FIELDS, SHOWN, STAGES, lazy_menu, library_surface, selected_loading


class WorldLibraryEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "loading").mkdir()
        header = bytearray(54)
        header[:2] = b"BM"
        struct.pack_into("<ii", header, 18, 1280, 720)
        for name in ["Checking-the-selected-world---", *[stage.replace(" ", "-") for stage in STAGES]]:
            (self.root / "loading" / (name + ".bmp")).write_bytes(header)
        (self.root / "frame.bmp").write_bytes(header)
        self.menu = "world_library_io phase=menu " + " ".join(field + "=0" for field in IO_FIELDS) + "\n"
        self.text = self.menu + SHOWN + "\n" + "\n".join(
            f"world_load_stage stage={stage} ui_safe={cpu}" for stage, cpu in STAGES.items())
        self.text += "\nmap_boot responsiveness=1 worker_jobs=1 result=ready\n"

    def test_complete_selected_evidence(self):
        self.assertEqual(selected_loading(self.text, self.root)["unselected_content_reads"], 0)

    def test_menu_counter_is_required(self):
        with self.assertRaises(AssertionError):
            lazy_menu("client_main_menu ready=1")

    def test_each_unselected_operation_is_rejected(self):
        for field in IO_FIELDS:
            with self.subTest(field=field), self.assertRaises(AssertionError):
                lazy_menu(self.menu.replace(field + "=0", field + "=1"))

    def test_unselected_renderer_load_is_rejected(self):
        with self.assertRaises(AssertionError):
            lazy_menu(self.menu + "map_renderer_loaded vertices=8")

    def test_stale_loading_overlay_is_rejected(self):
        for text in ("", "world_library_surface visible=0 loading=1\n", "world_library_surface visible=1 loading=1\n"):
            with self.subTest(text=text), self.assertRaises(AssertionError):
                library_surface(text)

    def test_gpu_phase_cannot_claim_cpu_lease(self):
        with self.assertRaises(AssertionError):
            selected_loading(self.text.replace("Uploading textures ui_safe=0", "Uploading textures ui_safe=1"), self.root)

    def test_missing_loading_frame_is_rejected(self):
        (self.root / "loading/Preparing-geometry.bmp").unlink()
        with self.assertRaises(AssertionError):
            selected_loading(self.text, self.root)

    def test_loading_must_precede_native_work(self):
        with self.assertRaises(AssertionError):
            selected_loading("world_load_stage stage=Reading map ui_safe=1\n" + self.text, self.root)

    def test_failed_selection_returns_library_without_authority(self):
        text = self.menu + SHOWN + "\nworld_library_ready entries=1 status=failed\nworld_library_surface visible=1 loading=0\n"
        self.assertIn("failed", selected_loading(text, self.root, failed=True)["outcome"])
        with self.assertRaises(AssertionError):
            selected_loading(text + "authoritative_player_ready", self.root, failed=True)


if __name__ == "__main__":
    unittest.main()
