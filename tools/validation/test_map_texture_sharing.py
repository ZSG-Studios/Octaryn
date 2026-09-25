"""CPU source contracts for logical texture slots and content-keyed uploads."""
from pathlib import Path
import hashlib
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'octaryn-client/Source/MapWorld/MapRendererTextures.cpp'


class MapTextureSharing(unittest.TestCase):
    def setUp(self):
        self.source = SOURCE.read_text()

    def test_shares_both_objects_before_cache_read(self):
        alias = self.source.index('const auto previous=uploaded.find(key);')
        cache = self.source.index('read_map_texture_cache(')
        branch = self.source[alias:cache]
        self.assertIn('map.textures[slot]=map.textures[previous->second]', branch)
        self.assertIn('map.texture_views[slot]=map.texture_views[previous->second]', branch)
        self.assertIn('++stats.shared;return true;', branch)
        self.assertNotIn('stats.bytes', branch)

    def test_hash_once_and_record_cached_and_uncooked_success(self):
        self.assertEqual(self.source.count('map_texture_cache_key(source,options)'), 1)
        self.assertEqual(self.source.count('uploaded.emplace(key,slot)'), 2)
        self.assertIn('if(!key.empty() && !map.texture_cache_directory.empty())', self.source)
        self.assertIn('if(!key.empty())uploaded.emplace(key,slot)', self.source)

    def test_preserves_logical_slots_and_opacity_decode(self):
        self.assertIn('std::map<std::pair<size_t,MapMipOptions>,size_t> variants', self.source)
        self.assertIn('map.material_texture_slots[primitive][i]=entry->second', self.source)
        self.assertLess(self.source.index('decode_map_image('), self.source.index('!upload(map,'))
        self.assertLess(self.source.index('decoded.rgba[pixel]!=255'), self.source.index('!upload(map,'))
        self.assertIn('shared_uploads=%u gpu_bytes=%llu', self.source)

    def test_identity_key_includes_content_and_all_mip_options(self):
        key_source = (SOURCE.parent / 'MapTextureHash.cpp').read_text()
        function = key_source[key_source.index('std::string map_texture_cache_key'):]
        for field in ('role', 'alpha_weighted', 'preserve_coverage', 'alpha_cutoff', 'alpha_factor'):
            self.assertIn('options.' + field, function)
        self.assertIn('map_texture_cache_version', function)
        self.assertIn('image.bytes.begin(),image.bytes.end()', function)

    def test_representative_keys_keep_four_slots_but_three_uploads(self):
        # Independently reproduce the version-2 key layout, not GPU allocation.
        def key(content, role):
            header = struct.pack('<IIIIIff', 0x5a534754, 2, role, 0, 0, .5, 1)
            return hashlib.sha256(header + content).hexdigest()

        logical = [(b'image-a', 0), (b'image-a', 0), (b'image-a', 2), (b'image-b', 0)]
        uploads = {}
        slots = []
        for slot, (content, role) in enumerate(logical):
            identity = key(content, role)
            slots.append(uploads.setdefault(identity, slot))
        self.assertEqual(len(slots), 4)
        self.assertEqual(len(uploads), 3)
        self.assertEqual(slots, [0, 0, 2, 3])

    def test_external_images_are_loaded_and_empty_images_fail_before_upload(self):
        loader = (SOURCE.parent / 'MapModel.cpp').read_text()
        decoder = (SOURCE.parent / 'MapImages.cpp').read_text()
        self.assertIn('std::memcpy(target.bytes.data(),encoded.data(),encoded.size())', loader)
        self.assertIn('if(source.bytes.empty())', decoder)


if __name__ == '__main__':
    unittest.main()
