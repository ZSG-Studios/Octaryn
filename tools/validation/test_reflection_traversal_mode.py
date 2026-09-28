"""Traversal control resolves platform qualification without changing sampling."""
import unittest
from capture_map_world import map_only_traversal


class TraversalMode(unittest.TestCase):
    def test_windows_qualified_defaults(self):
        for backend in ('dx12', 'vulkan'):
            self.assertTrue(map_only_traversal(None,backend,False,'on','on','nt'))

    def test_other_platforms_and_backends_stay_generic(self):
        for backend,platform in (('vulkan','posix'),('metal','posix'),('metal','nt')):
            self.assertFalse(map_only_traversal(None,backend,False,'on','on',platform))

    def test_explicit_generic_control(self):
        for backend in ('dx12', 'vulkan'):
            self.assertFalse(map_only_traversal(False,backend,False,'on','on','nt'))

    def test_explicit_experimental_request(self):
        self.assertTrue(map_only_traversal(True,'vulkan',False,'on','on','posix'))

    def test_inactive_and_queue_defaults(self):
        for queued,temporal,rays in ((True,'on','on'),(False,'off','on'),(False,'on','off')):
            self.assertFalse(map_only_traversal(None,'dx12',queued,temporal,rays,'nt'))
            with self.assertRaises(ValueError):
                map_only_traversal(True,'dx12',queued,temporal,rays,'nt')


if __name__ == '__main__':
    unittest.main()
