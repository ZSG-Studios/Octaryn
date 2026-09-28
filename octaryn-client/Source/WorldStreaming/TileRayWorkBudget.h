#pragma once
#include "MapRayWork.h"
#include <cmath>
namespace octaryn::client::rendering {
struct TileRayWorkBudget {
  static constexpr unsigned max_capacity=4,operation_limit=1;
  double milliseconds;
  unsigned capacity{max_capacity};
  unsigned polls{},operations{},submissions{};
  bool poll() {if(polls>=capacity)return false;++polls;return true;}
  bool allows(MapRayStep step,unsigned in_flight,double elapsed_ms) const {
    return map_ray_step_has_work(step) && operations<operation_limit && std::isfinite(elapsed_ms) &&
        elapsed_ms<milliseconds && (step!=MapRayStep::Build || in_flight<capacity);
  }
  void record(MapRayStep step) {++operations;if(map_ray_step_submits(step))++submissions;}
};
inline bool tile_ray_capacity_valid(unsigned capacity) {return capacity==1 || capacity==4;}
}
