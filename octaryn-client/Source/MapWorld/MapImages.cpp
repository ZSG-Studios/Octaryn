#include "MapImages.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace octaryn::client::rendering {
bool decode_map_image(const MapModelImage& source,MapDecodedImage& output,std::string& error) {
  output=MapDecodedImage{};
  if(source.bytes.empty()) {error="map image has no bytes";return false;}
  int width{},height{},channels{};
  if(!stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(source.bytes.data()),
      static_cast<int>(source.bytes.size()),&width,&height,&channels) || width<=0 || height<=0) {
    error="map image is not a readable PNG or JPEG";return false;
  }
  if(width>4096 || height>4096) {error="map image exceeds the 4096 pixel dimension limit";return false;}
  auto* pixels=stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(source.bytes.data()),
      static_cast<int>(source.bytes.size()),&width,&height,&channels,4);
  if(!pixels) {error="map image decode failed";return false;}
  output.width=static_cast<std::uint32_t>(width);
  output.height=static_cast<std::uint32_t>(height);
  output.rgba.assign(pixels,pixels+static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*4);
  stbi_image_free(pixels);
  return true;
}
}
