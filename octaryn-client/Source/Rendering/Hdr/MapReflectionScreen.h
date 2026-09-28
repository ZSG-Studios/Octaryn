#pragma once
#include <slang-rhi.h>
#include <cstdlib>
#include <cmath>
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
inline bool map_reflection_screen_supported(rhi::IDevice* device) {
  static const bool native_geometry=[] {
    const char* value=std::getenv("OCTARYN_CLIENT_MAP_LOD_PIXELS");if(!value || !*value)return true;
    char* end=nullptr;const float pixels=std::strtof(value,&end);
    return end!=value && !*end && std::isfinite(pixels) && pixels==0;
  }();
  return map_reflection_screen_requested() && native_geometry && device->hasFeature(rhi::Feature::ConservativeRasterization);
}
struct WorldRenderer;
bool prepare_map_reflection_coverage(WorldRenderer&);
bool render_map_reflection_coverage(WorldRenderer&,rhi::ICommandEncoder*);
}
