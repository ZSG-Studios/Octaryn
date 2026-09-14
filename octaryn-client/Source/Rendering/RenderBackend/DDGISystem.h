#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <slang-rhi.h>

namespace octaryn::client::rendering {
struct WorldRenderer;
struct DDGIConfig {
  std::array<std::uint32_t,3> counts{16,8,16};
  float spacing{4},hysteresis{.94f},max_distance{64};
  std::uint32_t rays{112},budget{64},irradiance_resolution{6},visibility_resolution{8};
};
struct DDGIControl {
  std::array<std::int32_t,3> cell{};std::uint32_t version{1};
  std::uint32_t refresh_frame{};std::array<std::uint32_t,3> padding{};
};
struct DDGIProbe { float offset[4]{};std::uint32_t metadata[4]{}; };
struct DDGIStats {
  std::uint32_t updated_probes{},scheduled_rays{},probe_count{},invalidated_probes{};
  std::uint64_t bytes{};
};
struct DDGISystem {
  DDGIConfig config;
  DDGIStats stats;
  std::unique_ptr<DDGISystem> fine_volume;
  Slang::ComPtr<rhi::IBuffer> controls,probes,irradiance,distance,rays;
  std::array<Slang::ComPtr<rhi::IBuffer>,2> selections;
  Slang::ComPtr<rhi::IComputePipeline> trace,update;
  std::vector<DDGIControl> control_data;
  std::vector<std::uint64_t> last_updates;
  std::vector<bool> dirty;
  std::vector<std::uint32_t> selected;
  std::array<std::int32_t,3> origin{};
  std::array<float,3> fade_origin{};
  bool available{},initialized{},controls_dirty{true},cell_centered{};
  std::uint64_t frame{},scene_revision{},light_revision{};
};
bool world_ddgi_initialize(WorldRenderer&);
bool world_ddgi_update(WorldRenderer&,rhi::ICommandEncoder*);
bool world_ddgi_bind(WorldRenderer&,rhi::IShaderObject*);
void ddgi_schedule(DDGISystem&,const std::array<float,3>& camera);
void ddgi_invalidate(DDGISystem&,const std::array<float,3>& minimum,const std::array<float,3>& maximum);
}
