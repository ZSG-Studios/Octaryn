#pragma once
#include "MapTextureCache.h"
namespace octaryn::client::rendering {
MapCachedTexture encode_map_bc7(const std::vector<MapDecodedImage>& levels,bool srgb);
bool test_map_texture_cook(const std::filesystem::path&);
bool compare_map_texture_caches(const std::filesystem::path&,const std::filesystem::path&,const std::filesystem::path&);
}
