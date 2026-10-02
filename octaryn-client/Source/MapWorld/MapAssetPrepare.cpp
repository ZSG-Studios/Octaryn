#include "MapAssetPrepare.h"
#include "MapMeshOptimization.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string_view>

namespace octaryn::client::rendering {
bool prepare_map_images(MapModel& model,const std::filesystem::path& cache,PreparedMapImages& output,std::string& error,
    const std::atomic_bool* cancel,std::uint64_t budget,bool cooked_required,const MapTextureReuseIndex* reuse) {
  PreparedMapImages result;
  std::uint64_t retained{};
  std::map<std::pair<size_t,MapMipOptions>,size_t> requests;
  for(const auto& primitive:model.primitives)for(unsigned role=0;role<21;++role) {
    const auto image=primitive.material.textures[role].image;if(image<0)continue;
    requests.emplace(std::make_pair(size_t(image),map_mip_options(primitive.material,role)),0);
  }
  std::map<std::string,size_t> unique;
  std::vector<bool> opaque(model.images.size());
  for(size_t index=0;index<model.images.size();++index) {
    if(cancel && cancel->load(std::memory_order_relaxed)) {error="map preparation cancelled";return false;}
    MapDecodedImage decoded;
    for(auto& [request,slot]:requests) {
      if(request.first!=index)continue;
      if(cancel && cancel->load(std::memory_order_relaxed)) {error="map preparation cancelled";return false;}
      const auto& options=request.second;
      const bool color=options.role==MapMipRole::BaseColor || options.role==MapMipRole::Emissive;
      const auto key=map_texture_cache_key(model.images[index],options);
      if(const auto previous=unique.find(key);previous!=unique.end()) {
        slot=previous->second;opaque[index]=result.textures[slot].texture.opaque;++result.shared;continue;
      }
      PreparedMapTexture prepared;prepared.key=key;
      if(reuse)prepared.resident=reuse->lookup(cache,key);
      if(prepared.resident) {
        const auto& metadata=prepared.resident->metadata;
        if(metadata.srgb!=color) {error="resident texture color role mismatch";return false;}
        prepared.texture.srgb=metadata.srgb;prepared.texture.compressed=metadata.compressed;
        prepared.texture.opaque=metadata.opaque;prepared.cached=true;
        ++result.resident_reuses;result.avoided_payload_bytes+=metadata.bytes;
      } else {
        const auto status=read_map_texture_cache(cache/(key+".dds"),0,0,color,prepared.texture,error,budget-retained);
        if(status==MapCacheResult::Invalid)return false;
        prepared.cached=status==MapCacheResult::Ready;
      }
      if(!prepared.cached) {
        if(cooked_required) {error="HQ200 requires validated cooked textures";return false;}
        if(decoded.rgba.empty()) {
          if(!decode_map_image(model.images[index],decoded,error))return false;
          ++result.decoded;
        }
        prepared.texture=lossless_map_texture_cache(build_map_mips(decoded,options),color);
        prepared.texture.opaque=true;
        for(size_t pixel=3;pixel<decoded.rgba.size();pixel+=4)
          if(decoded.rgba[pixel]!=255) {prepared.texture.opaque=false;break;}
      }
      opaque[index]=prepared.texture.opaque;
      for(const auto& mip:prepared.texture.levels)retained+=mip.blocks.size();
      if(retained>budget) {error="prepared map textures exceed CPU budget";return false;}
      slot=result.textures.size();unique.emplace(key,slot);result.textures.push_back(std::move(prepared));
    }
    std::vector<std::uint8_t>().swap(model.images[index].bytes);
  }
  result.variants=static_cast<unsigned>(requests.size());result.slots.resize(model.primitives.size());
  for(size_t p=0;p<model.primitives.size();++p) {
    auto& material=model.primitives[p].material;
    for(unsigned role=0;role<21;++role)if(material.textures[role].image>=0)
      result.slots[p][role]=requests.at({size_t(material.textures[role].image),map_mip_options(material,role)});
    if(material.alpha_mode!=MapAlphaMode::Blend || material.view_fade || material.additive || material.base_color[3]!=1 ||
        (material.texture>=0 && !opaque[size_t(material.texture)]))continue;
    bool vertex_opaque=true;const auto& primitive=model.primitives[p];
    for(unsigned i=0;i<primitive.index_count;++i)
      if(model.vertices[model.indices[primitive.first_index+i]].color[3]!=1) {vertex_opaque=false;break;}
    if(vertex_opaque) {material.alpha_mode=MapAlphaMode::Opaque;++result.promoted;}
  }
  output=std::move(result);error.clear();return true;
}
bool prepare_map_asset(const std::filesystem::path& source,PreparedMapAsset& output,std::string& error,const std::atomic_bool* cancel,
    const std::filesystem::path& shared_cache,const MapTextureReuseIndex* reuse,const MapLoadLimits* load_limits) {
  PreparedMapAsset asset;asset.source=source;
  constexpr std::uint64_t budget=256u*1024*1024;std::error_code ec;
  if(cancel && cancel->load(std::memory_order_relaxed)) {error="map preparation cancelled";return false;}
  const auto source_bytes=std::filesystem::file_size(source,ec);
  const auto prepared_bytes=load_limits && load_limits->source_length?load_limits->source_length:source_bytes;
  if(prepared_bytes>budget || ec) {error="tile source exceeds CPU preparation limit";return false;}
  const auto* profile=std::getenv("OCTARYN_CLIENT_PERFORMANCE_PROFILE");
  const bool bounded=profile && std::string_view(profile)=="HQ200";
  MapLoadLimits limits;
  if(bounded)limits={16ull*1024*1024,64ull*1024*1024,16384,2048,49152};
  if(load_limits)limits=*load_limits;
  limits.cancel=cancel;limits.geometry_bytes=std::min(limits.geometry_bytes,budget);
  if(!load_map_model(source,asset.model,error,limits))return false;
  const auto geometry_bytes=asset.model.vertices.size()*sizeof(MapVertex)+
      (asset.model.indices.size()+asset.model.collision_indices.size())*4;
  if(geometry_bytes>budget) {error="tile geometry exceeds CPU preparation limit";return false;}
  if(cancel && cancel->load(std::memory_order_relaxed)) {error="map preparation cancelled";return false;}
  if(!optimize_map_mesh(asset.model,error))return false;
  auto cache=source;cache+=".textures";
  if(!shared_cache.empty())cache=shared_cache;
  asset.texture_cache=cache;
  const auto lod_bytes=asset.lods.indices.size()*4+asset.lods.primitives.size()*sizeof(std::array<MapLodLevel,2>);
  if(geometry_bytes+lod_bytes>budget) {error="tile LOD exceeds CPU preparation limit";return false;}
  if(!prepare_map_images(asset.model,cache,asset.images,error,cancel,
      bounded?96ull*1024*1024:budget-geometry_bytes-lod_bytes,bounded,reuse))return false;
  if(!virtual_geometry::prepare_map_geometry(source,asset.model,asset.geometry_cache,error))return false;
  std::uint64_t forward_budget=budget-geometry_bytes-lod_bytes;
  for(const auto& image:asset.images.textures)for(const auto& level:image.texture.levels) {
    if(level.blocks.size()>forward_budget) {error="tile prepared images exceed CPU budget";return false;}
    forward_budget-=level.blocks.size();
  }
  if(!build_map_forward_geometry(asset.model,asset.forward,error,forward_budget))return false;
  output=std::move(asset);return true;
}
}
