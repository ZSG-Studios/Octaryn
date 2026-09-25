#include "GICoverage.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace octaryn::client::rendering {
std::vector<GIEmitterCell> build_gi_emitter_cells(std::span<const WorldLocalLight> lights) {
  std::vector<GIEmitterCell> cells;
  // Invalid oversized input must preserve ray emission, never falsely suppress it.
  if(lights.size()>gi_max_emitter_cells)return cells;
  for(const auto& light:lights) {
    if(light.axis_v_type[3]!=3 || !std::isfinite(light.position_range[3]) || light.position_range[3]<=0)continue;
    bool valid=true;double energy=0;
    for(unsigned c=0;c<3;++c) {
      valid=valid && std::isfinite(light.color_intensity[c]) && light.color_intensity[c]>=0;
      energy+=light.color_intensity[c];
    }
    valid=valid && std::isfinite(light.color_intensity[3]) && light.color_intensity[3]>0 && energy>0;
    GIEmitterCell cell{};
    for(unsigned axis=0;axis<3;++axis) {
      const double value=std::floor(double(light.position_range[axis]));
      if(!std::isfinite(value) || value<std::numeric_limits<std::int32_t>::min() ||
          value>std::numeric_limits<std::int32_t>::max()) {valid=false;break;}
      cell[axis]=static_cast<std::int32_t>(value);
    }
    if(valid)cells.push_back(cell);
  }
  std::sort(cells.begin(),cells.end());cells.erase(std::unique(cells.begin(),cells.end()),cells.end());
  return cells;
}
}
