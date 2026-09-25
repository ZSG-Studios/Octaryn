#include "AtlasInternal.h"
#include "AssetPath.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
namespace octaryn::client::rendering {
std::string atlas_asset_path(const char* relative,bool asset) {
  char path[2048]{};
  if (!(asset?asset_path_build(path,sizeof(path),relative):bundle_path_build(path,sizeof(path),relative))) return {};
  return path;
}
SDL_Surface* load_atlas_rgba(const char* relative) {
  const auto path=atlas_asset_path(relative);
  SDL_Surface* image=path.empty()?nullptr:SDL_LoadPNG(path.c_str());
  if (!image) { std::fprintf(stderr,"Atlas load failed: %s: %s\n",path.c_str(),SDL_GetError()); return nullptr; }
  if (image->format==SDL_PIXELFORMAT_RGBA32) return image;
  auto* converted=SDL_ConvertSurface(image,SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(image);
  return converted;
}
std::vector<unsigned char> atlas_layer_pixels(const SDL_Surface* image,unsigned layer,atlas_mip_kind_t kind) {
  std::array<unsigned char,32*32*4> current{},next{};
  for (int y=0;y<32;++y) std::memcpy(current.data()+y*128,
      static_cast<const unsigned char*>(image->pixels)+y*image->pitch+layer*128,128);
  std::vector<unsigned char> data((1024+256+64+16+4+1)*4);
  Uint32 offset{};
  atlas_pack_layer_mips(data.data(),&offset,current.data(),next.data(),ATLAS_MIP_LEVELS,kind);
  return data;
}
std::vector<rhi::SubresourceData> atlas_subresources(
    const std::vector<unsigned char>& data,unsigned layers) {
  std::vector<rhi::SubresourceData> subresources;
  std::size_t offset{};
  for (unsigned layer=0;layer<layers;++layer) for (int size=32;size;size/=2) {
    subresources.push_back({data.data()+offset,static_cast<rhi::Size>(size*4),static_cast<rhi::Size>(size*size*4)});
    offset+=static_cast<std::size_t>(size*size*4);
  }
  return subresources;
}
}
