#pragma once
#include "MapModel.h"
#include <cstdint>
#include <string>
#include <vector>

namespace octaryn::client::rendering {
// Straight (non-premultiplied) RGBA8 pixels; sRGB decoding stays in the shader.
struct MapDecodedImage {
  std::uint32_t width{},height{};
  std::vector<std::uint8_t> rgba;
};
bool decode_map_image(const MapModelImage&,MapDecodedImage&,std::string& error);
}
