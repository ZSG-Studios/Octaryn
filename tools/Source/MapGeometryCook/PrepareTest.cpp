#include "MapAssetPrepare.h"
#include <fstream>
#include <stdexcept>
#include <cstdio>

namespace octaryn::client::rendering {
// The preparation target never links GPU owners. This stand-in proves opaque
// ticket lifetime without constructing or calling a graphics resource.
struct MapTextureResource {};
bool test_virtual_map_cache(const std::filesystem::path&);
bool test_map_asset_prepare(const std::filesystem::path& root) {
  const auto require=[](bool okay,const char* reason) {if(!okay)throw std::runtime_error(reason);};
  {
    MapModel geometry;geometry.vertices.resize(5);geometry.indices={0,1,2,2,3,4,4,3,2};geometry.primitives.resize(3);
    for(unsigned p=0;p<3;++p) {geometry.primitives[p].first_index=p*3;geometry.primitives[p].index_count=3;}
    geometry.primitives[1].material.alpha_mode=geometry.primitives[2].material.alpha_mode=MapAlphaMode::Blend;
    geometry.vertices[4].position[0]=17;
    MapForwardGeometry forward;std::string forward_error;
    require(build_map_forward_geometry(geometry,forward,forward_error),"forward extraction failed");
    require(forward.vertices.size()==3 && forward.indices==std::vector<std::uint32_t>({0,1,2,2,1,0}) &&
        forward.first_indices==std::vector<std::uint32_t>({UINT32_MAX,0,3}) && forward.vertices[2].position[0]==17,
        "forward extraction did not isolate BLEND or preserve shared vertices/material offsets");
    require(geometry.indices==std::vector<std::uint32_t>({0,1,2,2,3,4,4,3,2}) && geometry.primitives[1].first_index==3,
        "forward extraction modified authoritative source geometry");
    require(!build_map_forward_geometry(geometry,forward,forward_error,1),"forward budget ignored");
    for(auto& primitive:geometry.primitives)primitive.material.alpha_mode=MapAlphaMode::Mask;
    require(build_map_forward_geometry(geometry,forward,forward_error,0) && forward.vertices.empty() && forward.indices.empty(),
        "opaque/masked geometry retained indexed GPU payload");
    std::printf("map_forward_geometry_tests passed=1 blend_only=1 shared_vertices=1 source_unchanged=1 bounded=1\n");
  }
  const std::vector<std::uint8_t> png{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,0,0,0,15,73,68,65,84,120,156,99,248,143,6,24,72,23,0,0,120,60,63,193,88,238,41,168,0,0,0,0,73,69,78,68,174,66,96,130};
  const auto model=[&] {
    MapModel value;value.images={{png,"image/png"},{png,"image/png"}};value.vertices.resize(3);
    value.indices={0,1,2,0,1,2};value.primitives.resize(2);
    for(unsigned p=0;p<2;++p) {
      auto& primitive=value.primitives[p];primitive.first_index=p*3;primitive.index_count=3;
      primitive.material.alpha_mode=MapAlphaMode::Blend;primitive.material.texture=p;primitive.material.textures[0].image=p;
    }
    return value;
  };
  auto first=model();PreparedMapImages prepared;std::string error;
  auto uncooked=root/"uncooked";
  require(prepare_map_images(first,uncooked,prepared,error),"CPU preparation failed");
  require(prepared.decoded==1 && prepared.textures.size()==1 && prepared.shared==1 && prepared.promoted==2,"texture dedup or alpha parity");
  require(prepared.slots[0][0]==prepared.slots[1][0] && first.images[0].bytes.empty() && first.images[1].bytes.empty(),"retained source storage");
  std::filesystem::create_directories(root/"cooked");
  require(write_map_texture_cache(root/"cooked"/(prepared.textures[0].key+".dds"),prepared.textures[0].texture,error),"write prepared cache");
  auto second=model();PreparedMapImages cached;
  require(prepare_map_images(second,root/"cooked",cached,error) && cached.decoded==0 && cached.textures[0].cached,"cache path decoded source");
  require(cached.textures[0].texture.levels[0].blocks==prepared.textures[0].texture.levels[0].blocks,"cooked/uncooked mismatch");
  std::atomic_bool cancelled{true};auto third=model();
  require(!prepare_map_images(third,uncooked,cached,error,&cancelled),"cancelled preparation ran");
  auto fourth=model();
  require(!prepare_map_images(fourth,uncooked,cached,error,nullptr,1),"prepared memory cap ignored");
  const auto cache=root/"resident-only";
  auto resource=std::make_shared<MapTextureResource>();std::weak_ptr<MapTextureResource> weak=resource;
  MapTextureReuseIndex index;index.cache_directory=cache;
  const auto& original=prepared.textures[0];std::uint64_t payload=0;
  for(const auto& level:original.texture.levels)payload+=level.blocks.size();
  index.entries.emplace(original.key,MapTextureReuseIndex::Entry{resource,
      {true,false,true,4,4,unsigned(original.texture.levels.size()),payload}});
  auto reused_model=model();PreparedMapImages reused;
  require(prepare_map_images(reused_model,cache,reused,error,nullptr,1,true,&index),"resident hit read missing DDS");
  require(reused.resident_reuses==1 && reused.avoided_payload_bytes==payload && reused.decoded==0 &&
      reused.shared==1 && reused.promoted==2 && reused.textures[0].texture.levels.empty(),"resident metadata parity");
  require(!index.lookup(root/"different-cache",original.key) && !index.lookup(cache,"other-key"),"cache namespace collision");
  auto other_model=model();PreparedMapImages failed;
  require(!prepare_map_images(other_model,root/"different-cache",failed,error,nullptr,1024,true,&index) &&
      error=="HQ200 requires validated cooked textures","namespace miss bypassed strict validation");
  auto partial_model=model();partial_model.images[1].bytes.push_back(1);
  require(!prepare_map_images(partial_model,cache,failed,error,nullptr,1024,true,&index),"partial miss ignored");
  require(resource.use_count()==2,"failed preparation retained acquired ticket");
  auto cancelled_model=model();
  require(!prepare_map_images(cancelled_model,cache,failed,error,&cancelled,1024,true,&index) &&
      resource.use_count()==2,"cancelled preparation retained ticket");
  resource.reset();
  require(!weak.expired(),"evicted owner destroyed prepared texture");
  auto second_ticket=index.lookup(cache,original.key);reused={};
  require(!weak.expired(),"second prepared consumer lost ownership");
  second_ticket.reset();require(weak.expired(),"weak snapshot pinned evicted texture");
  auto expired_model=model();
  require(!prepare_map_images(expired_model,cache,failed,error,nullptr,1024,true,&index),"expired snapshot bypassed validation");
  // A miss must continue through the production receipt/hash validation.
  {std::ofstream corrupt(root/"cooked"/(original.key+".dds"),std::ios::binary|std::ios::trunc);corrupt<<"bad";}
  auto corrupt_model=model();
  require(!prepare_map_images(corrupt_model,root/"cooked",failed,error,nullptr,1024,true,&index),"corrupt cache accepted");
  std::printf("map_texture_reuse_prepare_tests passed=1 avoided_dds_bytes=%llu\n",static_cast<unsigned long long>(payload));
  std::printf("map_asset_prepare_tests passed=1\n");return test_virtual_map_cache(root);
}
}
