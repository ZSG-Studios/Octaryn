#pragma once
namespace octaryn::client::rendering {
enum class MapRayStep {Build=0,BuildFence=1,CompactAllocate=2,CompactAllocation=3,
                      CompactSubmit=4,CompactFence=5,Ready=6};
inline bool map_ray_step_has_work(MapRayStep step) {
  return step==MapRayStep::Build || step==MapRayStep::CompactAllocate || step==MapRayStep::CompactSubmit;
}
inline bool map_ray_step_submits(MapRayStep step) {
  return step==MapRayStep::Build || step==MapRayStep::CompactSubmit;
}
}
