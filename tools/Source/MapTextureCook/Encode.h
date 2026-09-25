#pragma once
#include "MapTextureCache.h"
namespace map_texture_cook {
using namespace octaryn::client::rendering;
struct Quality {double squared_error{},normal_degrees{};unsigned maximum_error{},pixels{},mask_changed{};float maximum_normal_degrees{};};
bool encode(const std::vector<MapDecodedImage>&,const MapMipOptions&,MapCachedTexture&,Quality&,std::string&);
bool self_test(const std::filesystem::path&);
}
