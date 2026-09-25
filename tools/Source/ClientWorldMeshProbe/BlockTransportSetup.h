#pragma once
#include "Probe.h"
#include "BlockTransportGroups.h"
#include "WorldAtlas.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <thread>

namespace mesh_probe {
class BlockTransportSetup {
  using Key=std::pair<std::string,std::string>;
  struct Entry {Slang::ComPtr<rhi::IComputePipeline> pipeline;unsigned uses{};};
  std::map<Key,Entry> pipelines;
  rhi::IDevice* device;
  BlockTransportGroup group;
  static inline BlockTransportSetup* active=nullptr;
  static Key key(const char* path,const char* entry) {
    return {std::filesystem::path(path).lexically_normal().generic_string(),entry};
  }
  void prepare(const std::string& path,const char* entry="main") {
    Entry value;
    require(create_rhi_compute_pipeline(device,path.c_str(),entry,value.pipeline),
        "BTGI pre-frame pipeline preparation");
    require(pipelines.emplace(key(path.c_str(),entry),std::move(value)).second,
        "BTGI duplicate prepared pipeline");
  }
public:
  explicit BlockTransportSetup(WorldRenderer& r,const BlockTransportGroup& selected):device(r.device.get()),group(selected) {
    require(!active && r.frames==0,"BTGI pipeline preparation must precede the first frame");
    const auto directory=std::filesystem::path(__FILE__).parent_path();
    const auto production=[&](const char* name) {
      prepare(std::string("octaryn-client/Shaders/BlockTransportGI/")+name+".slang");
    };
    const auto probe=[&](const char* name) {
      prepare((directory/(std::string("BlockTransport")+name+".slang")).generic_string());
    };
    const auto is=[&](const char* name){return std::string_view(group.name)==name;};
    if(is("cache") || is("room") || is("rays") || is("convergence") || is("leaf") || is("actor") || is("local-area"))
      for(const char* entry:{"count_main","prefix_main","select_main"})
        prepare("octaryn-client/Shaders/BlockTransportGI/Select.slang",entry);
    if(is("cache")) {production("Evict");production("AdmitContributors");probe("Cache");probe("Select");}
    if(is("admission")) {production("Admit");production("Evict");probe("WorldAdmission");}
    if(is("room") || is("convergence") || is("actor"))
      for(const char* name:{"Trace","ResolveContributors","Direct"})production(name);
    if(is("room") || is("convergence"))production("Bounce");
    if(is("room")) {production("Admit");probe("Sampling");}
    if(is("raster"))probe("Reconstruction");
    if(is("rays")) {
      production("Trace");const auto path=(directory/"BlockTransportRay.slang").generic_string();
      prepare(path);prepare(path,"material_main");
    }
    if(is("plants")) {
      const auto path=(directory/"BlockTransportPlants.slang").generic_string();
      for(const char* entry:{"key_main","sample_main","ray_main"})prepare(path,entry);
      prepare("octaryn-client/Shaders/RayTracing/Shadow.slang");production("Bounce");probe("PlantEnergy");
    }
    if(is("numerical")) {production("Bounce");probe("Reflection");}
    if(is("sampling") || is("convergence") || is("actor"))probe("Sampling");
    if(is("actor"))probe("ActorHistory");
    if(is("boundary"))probe("Boundary");
    if(is("dynamic")) {production("DynamicReceivers");probe("Geometry");probe("ItemLighting");}
    if(is("leaf")) {production("Direct");production("Bounce");probe("Leaves");}
    if(is("local-area")) {production("Direct");production("Bounce");}
    require(pipelines.size()==group.pipelines,"BTGI fixed group pipeline inventory");
    if(group.rays) {
      r.ray_enabled=true;
      require(world_ray_initialize(r) && world_ray_available(r),"BTGI pre-frame RT pipeline preparation");
    }
    if(group.masks)require(prepare_world_atlas_plant_masks(r.atlas),"BTGI pre-frame plant mask preparation");
    active=this;
    std::printf("block_transport_setup=passed group=%s pipelines=%u ray_bounds=%u plant_masks=%u before_frames=1\n",
        group.name,group.pipelines,group.rays?1u:0u,group.masks?1u:0u);
  }
  BlockTransportSetup(const BlockTransportSetup&)=delete;
  BlockTransportSetup& operator=(const BlockTransportSetup&)=delete;
  ~BlockTransportSetup() {active=nullptr;}
  static bool lookup(rhi::IDevice* device,const char* path,const char* entry,
      Slang::ComPtr<rhi::IComputePipeline>& output) {
    require(active && active->device==device,"BTGI prepared pipeline owner mismatch");
    const auto found=active->pipelines.find(key(path,entry));
    require(found!=active->pipelines.end(),"BTGI unprepared pipeline requested during completed-frame checks");
    output=found->second.pipeline;++found->second.uses;return true;
  }
  void verify()const {
    unsigned uses=0;
    for(const auto& [identity,entry]:pipelines) {
      require(entry.uses>0,"BTGI prepared pipeline was not exercised by the production fixtures");
      uses+=entry.uses;
    }
    std::printf("block_transport_pipeline_reuse=passed group=%s pipelines=%zu uses=%u runtime_compiles=0\n",
        group.name,pipelines.size(),uses);
  }
};
inline bool block_transport_pipeline(rhi::IDevice* device,const char* path,const char* entry,
    Slang::ComPtr<rhi::IComputePipeline>& output) {
  return BlockTransportSetup::lookup(device,path,entry,output);
}
inline void block_transport_complete(WorldRenderer& r,std::chrono::steady_clock::time_point start) {
  std::this_thread::sleep_until(start+std::chrono::milliseconds(34));
  std::ofstream heartbeat("frame-timing.csv",std::ios::app);
  heartbeat<<r.frames++<<','<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
  heartbeat.flush();require(bool(heartbeat),"BTGI completed-work heartbeat write");
}
}
