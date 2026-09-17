#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace octaryn::client::rendering {
inline constexpr unsigned src_max_cascades=8;
struct SrcConfig {
  float spacing=1,contact_length=1,base_interval=4,interval_growth=4,decay=0.97f;
  unsigned cascades=5,angular_resolution=4,base_capacity=65536;
  unsigned visible_lifetime=2,secondary_lifetime=12;
  unsigned max_surface_rays=1048576,hash_search_limit=128;
  float max_trace_distance=4096,feedback=0.85f;
  float lod_radius=64,lod_blend=1.1f;
  std::uint64_t memory_limit=1024ull*1024*1024;
};
struct SrcCascade {
  unsigned probe_offset=0,capacity=0,direction_offset=0,directions=0;
  unsigned hash_offset=0,hash_capacity=0,resolution=0,padding=0;
};
struct SrcLayout {
  std::array<SrcCascade,src_max_cascades> cascades{};
  unsigned probes=0,directions=0,hash_entries=0;
  std::uint64_t bytes=0;
};
bool src_validate(const SrcConfig&,SrcLayout&,std::string& error);
bool src_environment_config(SrcConfig&,std::string& error);
}
