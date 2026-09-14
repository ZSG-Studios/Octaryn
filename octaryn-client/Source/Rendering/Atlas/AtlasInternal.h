#pragma once
#include "WorldAtlas.h"
#include "AtlasPixels.h"
#include <slang-com-ptr.h>
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace octaryn::client::rendering {
struct AtlasAnimation {
  unsigned layer{},first{},active{~0u},total{};
  std::vector<unsigned> ticks;
  std::vector<std::vector<unsigned char>> frames;
};
struct WorldAtlas {
  Slang::ComPtr<rhi::IDevice> device;
  std::array<Slang::ComPtr<rhi::ITexture>,3> textures;
  std::array<Slang::ComPtr<rhi::ITextureView>,3> views;
  Slang::ComPtr<rhi::ISampler> cutout,linear,nearest,sprite;
  Slang::ComPtr<rhi::IBuffer> materials;
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> animation{nullptr,SDL_DestroySurface};
  std::vector<AtlasAnimation> animations;
  std::vector<unsigned> preview_layers;
};
std::string atlas_asset_path(const char* relative,bool asset=true);
SDL_Surface* load_atlas_rgba(const char* relative);
std::vector<unsigned char> atlas_layer_pixels(const SDL_Surface*,unsigned,atlas_mip_kind_t);
std::vector<rhi::SubresourceData> atlas_subresources(const std::vector<unsigned char>&,unsigned layers);
bool load_atlas_materials(WorldAtlas&);
bool load_atlas_animations(WorldAtlas&);
}
