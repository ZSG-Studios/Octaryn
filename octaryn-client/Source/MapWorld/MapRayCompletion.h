#pragma once
#include <cstdint>
namespace octaryn::client::rendering {
enum class MapRayCompletion {Pending,Complete,Failed};
inline MapRayCompletion map_ray_completion(bool valid,std::uint64_t value,double elapsed_seconds) {
  if(!valid || value==UINT64_MAX)return MapRayCompletion::Failed;
  if(value>=1)return MapRayCompletion::Complete;
  return elapsed_seconds>1?MapRayCompletion::Failed:MapRayCompletion::Pending;
}
}
