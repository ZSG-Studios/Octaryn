"""Registered dependency patch stacks remain exact when patches touch the same file."""
import difflib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build/support'))
from slang_rhi_patches import apply_registered_patches

class PatchStackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git('init', '-q')
        self.git('config', 'user.email', 'test@example.invalid')
        self.git('config', 'user.name', 'Patch fixture')
        self.git('config', 'core.autocrlf', 'false')
        (self.root / 'source.txt').write_text('original\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'base')
        self.patches = []
        for i, (before, after) in enumerate([('original\n', 'first\n'), ('first\n', 'second\n')]):
            patch = self.root / f'{i}.patch'
            patch.write_text('diff --git a/source.txt b/source.txt\n' + ''.join(difflib.unified_diff(
                before.splitlines(True), after.splitlines(True), fromfile='a/source.txt', tofile='b/source.txt')))
            self.patches.append(patch)
    def git(self, *args):
        return subprocess.run(['git', '-C', str(self.root), *args], check=True, capture_output=True).stdout
    def test_overlapping_stack_and_idempotence(self):
        apply_registered_patches(self.root, self.patches)
        apply_registered_patches(self.root, self.patches)
        self.assertEqual((self.root / 'source.txt').read_text(), 'second\n')
    def test_registered_prefix_can_advance(self):
        apply_registered_patches(self.root, self.patches[:1])
        apply_registered_patches(self.root, self.patches)
        self.assertEqual((self.root / 'source.txt').read_text(), 'second\n')
    def test_unregistered_tracked_change_rejected_unchanged(self):
        (self.root / 'source.txt').write_text('not registered\n')
        with self.assertRaises(ValueError):
            apply_registered_patches(self.root, self.patches)
        self.assertEqual((self.root / 'source.txt').read_text(), 'not registered\n')
    def test_unrelated_untracked_file_preserved(self):
        other = self.root / 'local.txt'
        other.write_text('keep\n')
        apply_registered_patches(self.root, self.patches)
        self.assertEqual(other.read_text(), 'keep\n')
        self.assertEqual(self.git('ls-files', 'local.txt'), b'')
if __name__ == '__main__':
    unittest.main()
