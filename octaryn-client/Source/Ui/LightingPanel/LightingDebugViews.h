#pragma once
#include <array>

namespace octaryn::client::app {
struct LightingDebugView {
  unsigned mode;
  const char* name;
};
inline constexpr std::array lighting_debug_views={
  LightingDebugView{0,"Off"},LightingDebugView{1,"Sun visibility"},
  LightingDebugView{2,"DDGI irradiance"},LightingDebugView{3,"DDGI distance"},
  LightingDebugView{4,"DDGI state"},LightingDebugView{5,"DDGI relocation"},
  LightingDebugView{6,"DDGI age"},LightingDebugView{7,"DDGI cells"},
  LightingDebugView{8,"Local light"},LightingDebugView{9,"TLAS instances"},
  LightingDebugView{10,"BLAS bounds"},LightingDebugView{11,"RT hit distance"},
  LightingDebugView{12,"Sun history"},LightingDebugView{13,"Light ID"},
  LightingDebugView{14,"Light age"},LightingDebugView{15,"Light M"},
  LightingDebugView{16,"Temporal acceptance"},LightingDebugView{17,"Spatial reuse"},
  LightingDebugView{18,"Light visibility"},LightingDebugView{19,"Weight sum"},
  LightingDebugView{20,"Light count"},LightingDebugView{21,"Probe irradiance"},
  LightingDebugView{22,"Probe state"},LightingDebugView{23,"Probe irradiance"},
  LightingDebugView{24,"Probe state"},LightingDebugView{25,"Probe convergence"},
  LightingDebugView{26,"Probe convergence"},LightingDebugView{27,"GI dirty regions"},
  LightingDebugView{28,"Sun direct"},LightingDebugView{29,"DDGI indirect"},
  LightingDebugView{30,"Ambient fallback"},LightingDebugView{31,"Sun terms"}};

constexpr unsigned sanitize_lighting_debug(unsigned mode) {
  for(const auto& view:lighting_debug_views)if(view.mode==mode)return mode;
  return 0;
}
constexpr unsigned next_lighting_debug(unsigned mode) {
  for(unsigned i=0;i<lighting_debug_views.size();++i)
    if(lighting_debug_views[i].mode==mode)
      return lighting_debug_views[(i+1)%lighting_debug_views.size()].mode;
  return 0;
}
constexpr const char* lighting_debug_name(unsigned mode) {
  for(const auto& view:lighting_debug_views)if(view.mode==mode)return view.name;
  return "Off";
}
}
