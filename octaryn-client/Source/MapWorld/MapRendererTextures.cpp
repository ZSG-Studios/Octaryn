#include "MapRendererInternal.h"
#include "MapMipmaps.h"
#include "MapTextureCache.h"
#include <algorithm>
#include <cstdio>
#include <map>

namespace octaryn::client::rendering {
namespace {
struct UploadStats {unsigned cached{},cached_rgba{},uncooked{},shared{};std::uint64_t bytes{};};
bool upload(MapRenderer& map,const MapDecodedImage& image,const MapModelImage& source,const MapMipOptions& options,
    size_t slot,UploadStats& stats,std::map<std::string,size_t>& uploaded) {
  const bool color=options.role==MapMipRole::BaseColor || options.role==MapMipRole::Emissive;
  const auto key=map_texture_cache_key(source,options);
  if(!key.empty()) {
    const auto previous=uploaded.find(key);
    if(previous!=uploaded.end()) {
      map.textures[slot]=map.textures[previous->second];
      map.texture_views[slot]=map.texture_views[previous->second];
      ++stats.shared;return true;
    }
  }
  if(!key.empty() && !map.texture_cache_directory.empty()) {
      MapCachedTexture cached;std::string error;
      const auto result=read_map_texture_cache(map.texture_cache_directory/(key+".dds"),image.width,image.height,color,cached,error);
      if(result==MapCacheResult::Invalid)std::fprintf(stderr,"map_texture_cache_invalid key=%s reason=%s\n",key.c_str(),error.c_str());
      if(result==MapCacheResult::Ready) {
        const auto format=cached.compressed?(color?rhi::Format::BC7UnormSrgb:rhi::Format::BC7Unorm):
            (color?rhi::Format::RGBA8UnormSrgb:rhi::Format::RGBA8Unorm);
        rhi::FormatSupport support{};
        const bool usable=(!cached.compressed || (image.width%4==0 && image.height%4==0)) &&
            SLANG_SUCCEEDED(map.device->getFormatSupport(format,&support)) && rhi::is_set(support,rhi::FormatSupport::ShaderSample);
        if(usable) {
        std::vector<rhi::SubresourceData> data;
        for(const auto& level:cached.levels)data.push_back({level.blocks.data(),
            static_cast<rhi::Size>(cached.compressed?(level.width+3)/4*16:level.width*4),static_cast<rhi::Size>(level.blocks.size())});
        rhi::TextureDesc desc{};desc.size={image.width,image.height,1};desc.format=format;
        desc.mipCount=static_cast<unsigned>(cached.levels.size());desc.sampleCount=1;
        desc.defaultState=rhi::ResourceState::ShaderResource;
        desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination;
        if(SLANG_SUCCEEDED(map.device->createTexture(desc,data.data(),map.textures[slot].writeRef())) &&
            SLANG_SUCCEEDED(map.textures[slot]->getDefaultView(map.texture_views[slot].writeRef()))) {
          if(cached.compressed)++stats.cached;else ++stats.cached_rgba;
          uploaded.emplace(key,slot);
          for(const auto& mip:cached.levels)stats.bytes+=mip.blocks.size();return true;
        }
        std::fprintf(stderr,"map_texture_cache_upload_failed key=%s\n",key.c_str());
        }
      }
  }
  const auto levels=build_map_mips(image,options);
  if(levels.empty())return false;
  std::vector<rhi::SubresourceData> data;
  for(const auto& level:levels) {
    data.push_back({level.rgba.data(),static_cast<rhi::Size>(level.width*4),static_cast<rhi::Size>(level.rgba.size())});
  }
  rhi::TextureDesc desc{};
  desc.size={image.width,image.height,1};desc.format=color?rhi::Format::RGBA8UnormSrgb:rhi::Format::RGBA8Unorm;
  desc.mipCount=static_cast<unsigned>(levels.size());desc.sampleCount=1;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopyDestination;
  if(SLANG_FAILED(map.device->createTexture(desc,data.data(),map.textures[slot].writeRef())) ||
      SLANG_FAILED(map.textures[slot]->getDefaultView(map.texture_views[slot].writeRef())))return false;
  if(!key.empty())uploaded.emplace(key,slot);
  ++stats.uncooked;for(const auto& mip:levels)stats.bytes+=mip.rgba.size();return true;
}
}
bool upload_map_images(MapRenderer& map) {
  UploadStats stats;
  std::map<std::string,size_t> uploaded;
  std::map<std::pair<size_t,MapMipOptions>,size_t> variants;
  map.material_texture_slots.resize(map.model.primitives.size());
  std::vector<bool> opaque(map.model.images.size());
  for(size_t primitive=0;primitive<map.model.primitives.size();++primitive)for(unsigned i=0;i<5;++i) {
    const auto& material=map.model.primitives[primitive].material;
    const auto image=material.textures[i].image;
    if(image<0)continue;
    const auto key=std::make_pair(static_cast<size_t>(image),map_mip_options(material,i));
    const auto entry=variants.emplace(key,variants.size()).first;
    map.material_texture_slots[primitive][i]=entry->second;
  }
  map.textures.resize(variants.size());map.texture_views.resize(variants.size());
  for(size_t index=0;index<map.model.images.size();++index) {
    bool used=false;for(const auto& [key,slot]:variants)if(key.first==index) {used=true;break;}
    if(!used)continue;
    MapDecodedImage decoded;std::string error;
    if(!decode_map_image(map.model.images[index],decoded,error)) {
      std::fprintf(stderr,"map_image_decode_failed index=%zu: %s\n",index,error.c_str());return false;
    }
    opaque[index]=true;
    for(size_t pixel=3;pixel<decoded.rgba.size();pixel+=4)
      if(decoded.rgba[pixel]!=255) {opaque[index]=false;break;}
    for(const auto& [key,slot]:variants)
      if(key.first==index && !upload(map,decoded,map.model.images[index],key.second,slot,stats,uploaded))return false;
  }
  unsigned promoted=0;
  for(auto& primitive:map.model.primitives) {
    auto& material=primitive.material;
    if(material.alpha_mode!=MapAlphaMode::Blend || material.base_color[3]!=1)continue;
    if(material.texture>=0 && !opaque[static_cast<size_t>(material.texture)])continue;
    bool vertex_opaque=true;
    for(unsigned i=0;i<primitive.index_count;++i)
      if(map.model.vertices[map.model.indices[primitive.first_index+i]].color[3]!=1) {vertex_opaque=false;break;}
    if(vertex_opaque) {material.alpha_mode=MapAlphaMode::Opaque;++promoted;}
  }
  std::printf("map_materials opaque_blend_promotions=%u\n",promoted);
  std::printf("map_textures variants=%zu cached_bc7=%u cached_rgba=%u uncooked_rgba=%u shared_uploads=%u gpu_bytes=%llu\n",
      variants.size(),stats.cached,stats.cached_rgba,stats.uncooked,stats.shared,static_cast<unsigned long long>(stats.bytes));
  return true;
}
}
