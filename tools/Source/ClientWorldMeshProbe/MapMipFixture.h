#pragma once
#include "../../../octaryn-client/Source/MapWorld/MapModel.h"
#include <utility>

namespace mesh_probe {
inline void initialize_map_mip_fixture(octaryn::client::rendering::MapModel& model) {
  using namespace octaryn::client::rendering;
  model={};
  // Four 4x4 PNGs: black/white, +X/+Z normals, roughness0/1, and MASK
  // quadrants with 0/1/3/4 opaque pixels (cutoff .9, coverage .5).
  const std::vector<std::uint8_t> encoded[]{
    {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,
      0,0,0,22,73,68,65,84,120,156,99,96,96,96,248,15,2,48,154,137,1,13,16,22,0,0,66,234,9,253,112,116,107,115,0,0,0,0,73,69,78,68,174,66,96,130},
    {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,
      0,0,0,28,73,68,65,84,120,156,99,252,223,208,240,191,145,161,158,161,158,161,145,1,68,51,49,160,1,194,2,0,115,98,6,6,112,13,58,99,0,0,0,0,73,69,78,68,174,66,96,130},
    {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,
      0,0,0,28,73,68,65,84,120,156,99,252,207,208,240,159,225,63,3,3,3,35,3,3,136,102,98,64,3,132,5,0,24,38,4,133,217,202,106,115,0,0,0,0,73,69,78,68,174,66,96,130},
    {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,
      0,0,0,33,73,68,65,84,120,156,99,100,248,207,0,3,32,22,35,19,156,203,192,192,8,34,88,160,50,112,0,83,1,150,5,49,0,153,100,3,10,76,164,83,250,0,0,0,0,73,69,78,68,174,66,96,130}};
  for(const auto& bytes:encoded) {MapModelImage image;image.bytes=bytes;image.mime_type="image/png";model.images.push_back(std::move(image));}
  model.primitives.resize(2);
  auto& material=model.primitives[0].material;
  material.textures[0].image=0;material.textures[1].image=2;material.textures[2].image=1;
  auto& mask=model.primitives[1].material;
  mask.alpha_mode=MapAlphaMode::Mask;mask.alpha_cutoff=.9f;mask.textures[0].image=mask.texture=3;
  for(auto& primitive:model.primitives)for(auto& texture:primitive.material.textures) {
    texture.min_filter=9984;texture.mag_filter=9728;texture.wrap_s=texture.wrap_t=33071;
  }
}
}
