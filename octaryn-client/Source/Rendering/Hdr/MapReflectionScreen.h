#pragma once
#include <slang-rhi.h>
#include <cstdlib>
namespace octaryn::client::rendering {
inline bool map_reflection_screen_requested() {
  static const bool requested=[] {
    const auto enabled=[](const char* name) {
      const char* value=std::getenv(name);return value && value[0]=='1' && value[1]=='\0';
    };
    return enabled("OCTARYN_CLIENT_RT_SCREEN_HITS") && enabled("OCTARYN_CLIENT_RT_QUEUED") &&
        !enabled("OCTARYN_CLIENT_RT_REFERENCE");
  }();
  return requested;
}
inline bool map_reflection_screen_supported(rhi::IDevice*) {
  // The raster DAG cut cannot certify a hit against the complete ray DAG cut.
  return false;
}
struct WorldRenderer;
bool prepare_map_reflection_coverage(WorldRenderer&);
bool render_map_reflection_coverage(WorldRenderer&,rhi::ICommandEncoder*);
}
