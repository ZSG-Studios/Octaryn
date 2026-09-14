#pragma once
#include <cstdint>
#include <memory>
#include <slang-rhi.h>
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldRayTracingStats {
  std::uint64_t blas_builds{},tlas_builds{},discarded_builds{};
  std::uint64_t blas_refits{},tlas_updates{},scene_generation{};
  std::uint64_t blas_bytes{},tlas_bytes{},temporary_bytes{};
  std::uint64_t retired_mesh_bytes{};
  double blas_gpu_ms{},tlas_gpu_ms{};
  std::uint32_t resident_columns{},ready_columns{},pending_columns{},active_jobs{};
};
class WorldRayTracing {
public:
  struct State;
  std::unique_ptr<State> state;
  WorldRayTracing();
  ~WorldRayTracing();
  WorldRayTracing(const WorldRayTracing&)=delete;
  WorldRayTracing& operator=(const WorldRayTracing&)=delete;
};
bool world_ray_initialize(WorldRenderer&);
// The caller must have completed the selected frame slot's fence before reuse.
bool world_ray_prepare(WorldRenderer&,rhi::ICommandEncoder*,unsigned slot);
bool world_ray_available(const WorldRenderer&);
bool world_ray_bind(WorldRenderer&,rhi::IShaderObject*);
void world_ray_set_build_budget(WorldRenderer&,unsigned builds_per_frame,unsigned faces_per_frame);
WorldRayTracingStats world_ray_stats(const WorldRenderer&);
}
