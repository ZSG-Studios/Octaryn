"""Exercise bundle replacement and failures using only disposable directories."""
import ctypes
import importlib.util
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "install_bundle", Path(__file__).resolve().parents[1] / "build/support/install_bundle.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


def contents(root):
    return {str(path.relative_to(root)): path.read_bytes()
            for path in root.rglob("*") if path.is_file()}


class BundleInstallTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="octaryn-bundle-test-")
        self.addCleanup(self.temporary.cleanup)
        self.bundle = Path(self.temporary.name).resolve() / "bundle"
        self.bundle.mkdir()
        (self.bundle / "client.dll").write_bytes(b"old")
        (self.bundle / "old-only.txt").write_bytes(b"preserve in retired")
        self.original = contents(self.bundle)
        self.stage = installer.prepare(self.bundle)
        (self.stage / "client.dll").write_bytes(b"new")
        (self.stage / "server").mkdir()
        (self.stage / "server/server.dll").write_bytes(b"complete server")
        self.expected = contents(self.stage)

    def test_prepare_only_cleans_staging(self):
        installer.prepare(self.bundle)
        self.assertEqual(contents(self.bundle), self.original)
        self.assertEqual(contents(self.stage), {})

    def test_server_world_survives_repeated_replacement(self):
        world = self.bundle / "octaryn-world"
        (world / "nested").mkdir(parents=True)
        (world / "nested" / "edits.bin").write_bytes(bytes(range(256)))
        (world / "player.json").write_bytes(b'{"x":123}')
        original = contents(self.bundle)
        retired = installer.install(self.bundle, "octaryn-world")
        self.assertEqual(contents(retired), original)
        self.assertTrue(installer.identical_trees(retired / "octaryn-world", world))
        installer.prepare(self.bundle)
        (self.stage / "server.dll").write_bytes(b"next build")
        previous = contents(self.bundle)
        second = installer.install(self.bundle, "octaryn-world")
        self.assertEqual(contents(second), previous)
        self.assertEqual(contents(retired), original)
        self.assertTrue(installer.identical_trees(retired / "octaryn-world", world))

    def test_world_copy_failure_restores_original_bundle(self):
        (self.bundle / "octaryn-world").mkdir()
        (self.bundle / "octaryn-world" / "edits.bin").write_bytes(b"authoritative edits")
        original = contents(self.bundle)
        with patch.object(installer.shutil, "copytree", side_effect=OSError("copy failed")):
            with self.assertRaises(OSError):
                installer.install(self.bundle, "octaryn-world")
        self.assertEqual(contents(self.bundle), original)

    def test_world_hash_mismatch_restores_original_bundle(self):
        (self.bundle / "octaryn-world").mkdir()
        (self.bundle / "octaryn-world" / "edits.bin").write_bytes(b"authoritative edits")
        original = contents(self.bundle)
        with patch.object(installer, "identical_trees", return_value=False):
            with self.assertRaises(ValueError):
                installer.install(self.bundle, "octaryn-world")
        self.assertEqual(contents(self.bundle), original)

    def test_staging_world_cannot_replace_user_world(self):
        (self.stage / "octaryn-world").mkdir()
        with self.assertRaises(ValueError):
            installer.install(self.bundle, "octaryn-world")
        self.assertEqual(contents(self.bundle), self.original)

    def test_install_preserves_entire_previous_bundle(self):
        retired = installer.install(self.bundle)
        self.assertEqual(contents(self.bundle), self.expected)
        self.assertEqual(contents(retired), self.original)
        self.assertFalse(self.stage.exists())

    def test_first_install(self):
        first = self.bundle.with_name("first")
        stage = installer.prepare(first)
        (stage / "client.dll").write_bytes(b"first")
        self.assertIsNone(installer.install(first))
        self.assertEqual(contents(first), {"client.dll": b"first"})

    def test_missing_or_empty_stage_does_not_touch_runtime(self):
        installer.prepare(self.bundle)
        with self.assertRaises(ValueError):
            installer.install(self.bundle)
        self.stage.rmdir()
        with self.assertRaises(ValueError):
            installer.install(self.bundle)
        self.assertEqual(contents(self.bundle), self.original)

    def test_blocked_retirement_preserves_both_trees(self):
        with patch.object(installer.os, "rename", side_effect=PermissionError("locked")):
            with self.assertRaises(PermissionError):
                installer.install(self.bundle)
        self.assertEqual(contents(self.bundle), self.original)
        self.assertEqual(contents(self.stage), self.expected)

    def test_failed_install_rolls_back_without_losing_staging(self):
        rename = os.rename
        def fail_install(source, target):
            if source == self.stage:
                raise PermissionError("injected stage rename failure")
            rename(source, target)
        with patch.object(installer.os, "rename", side_effect=fail_install):
            with self.assertRaises(PermissionError):
                installer.install(self.bundle)
        self.assertEqual(contents(self.bundle), self.original)
        self.assertEqual(contents(self.stage), self.expected)

    def test_rollback_failure_keeps_old_tree_for_recovery(self):
        rename = os.rename
        def fail_after_retirement(source, target):
            if source != self.bundle:
                raise PermissionError("injected destination lock")
            rename(source, target)
        with patch.object(installer.os, "rename", side_effect=fail_after_retirement):
            with self.assertRaisesRegex(RuntimeError, "previous bundle is intact"):
                installer.install(self.bundle)
        retired_root = self.bundle.with_name("bundle.retired")
        self.assertEqual(len(list(retired_root.iterdir())), 1)
        self.assertEqual(contents(next(retired_root.iterdir())), self.original)
        self.assertEqual(contents(self.stage), self.expected)

    def test_identical_tree_skips_rename_even_when_locked(self):
        installer.prepare(self.bundle)
        for name, data in self.original.items():
            (self.stage / name).write_bytes(data)
        with patch.object(installer.os, "rename", side_effect=AssertionError("must not rename")):
            self.assertEqual(installer.install(self.bundle), "unchanged")
        self.assertEqual(contents(self.bundle), self.original)
        self.assertFalse(self.bundle.with_name("bundle.retired").exists())

    def test_same_sizes_different_bytes_are_not_identical(self):
        self.assertFalse(installer.identical_trees(self.stage, self.bundle))
        installer.prepare(self.bundle)
        for name, data in self.original.items():
            (self.stage / name).write_bytes(data)
        (self.stage / "client.dll").write_bytes(b"NEW")
        self.assertFalse(installer.identical_trees(self.stage, self.bundle))

    @unittest.skipUnless(os.name == "posix", "POSIX executable permission semantics")
    def test_identical_bytes_still_repair_missing_executable_permission(self):
        installer.prepare(self.bundle)
        for name, data in self.original.items():
            (self.stage / name).write_bytes(data)
        (self.bundle / "client.dll").chmod(0o644)
        (self.stage / "client.dll").chmod(0o755)
        self.assertFalse(installer.identical_trees(self.stage, self.bundle))
        retired = installer.install(self.bundle)
        self.assertEqual(contents(self.bundle), self.original)
        self.assertEqual(stat.S_IMODE((self.bundle / "client.dll").stat().st_mode), 0o755)
        self.assertEqual(stat.S_IMODE((retired / "client.dll").stat().st_mode), 0o644)

    @unittest.skipUnless(os.name == "nt", "Windows directory sharing semantics")
    def test_real_windows_sharing_lock_preserves_runtime(self):
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
            ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
        handle = kernel.CreateFileW(str(self.bundle), 0x80000000, 3, None, 3, 0x02000000, None)
        self.assertNotEqual(handle, ctypes.c_void_p(-1).value)
        try:
            with self.assertRaises(PermissionError):
                installer.install(self.bundle)
            self.assertEqual(contents(self.bundle), self.original)
            self.assertEqual(contents(self.stage), self.expected)
        finally:
            kernel.CloseHandle(handle)


if __name__ == "__main__":
    unittest.main()
