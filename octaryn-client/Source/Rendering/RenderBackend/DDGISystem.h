#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>
#include <slang-rhi.h>

namespace octaryn::client::rendering {
struct WorldRenderer;
struct DDGIConfig {
  std::array<std::uint32_t,3> counts{32,12,32};
  float spacing{8},hysteresis{.94f},max_distance{96};
  std::uint32_t rays{112},budget{96},irradiance_resolution{6},visibility_resolution{8};
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
struct DDGIDirtyBox {
  std::uint64_t frame{};
  std::array<float,3> minimum{},maximum{};
};
struct DDGISystem {
  DDGIConfig config;
  DDGIStats stats;
  std::unique_ptr<DDGISystem> fine_volume;
  Slang::ComPtr<rhi::IBuffer> controls,probes,irradiance,distance,rays,variability;
  std::array<Slang::ComPtr<rhi::IBuffer>,2> selections;
  Slang::ComPtr<rhi::IComputePipeline> trace,update,seed;
  Slang::ComPtr<rhi::IRenderPipeline> debug;
  std::vector<DDGIControl> control_data;
  std::vector<std::uint64_t> last_updates;
  std::vector<bool> dirty;
  std::vector<std::uint32_t> selected;
  std::vector<std::uint8_t> occupancy;
  std::array<std::int32_t,3> origin{};
  std::array<float,3> fade_origin{};
  float env_spacing{8};
  std::array<int,3> ignore_voxel{std::numeric_limits<int>::max(),std::numeric_limits<int>::max(),
    std::numeric_limits<int>::max()};
  bool available{},initialized{},controls_dirty{true},cell_centered{};
  bool ignore_active{},ignore_held{};
  std::uint32_t ignore_released{};
  std::uint64_t frame{},scene_revision{},light_revision{},ignore_revision{};
  // Last published light influence bounds (position xyz, reach w) so removed or
  // moved lights also wake exactly the region they used to touch.
  std::vector<std::array<float,4>> light_bounds;
  std::uint64_t light_consumed_frame{};
  // Recent invalidation regions for the dirty-region debug view (mode 27).
  std::array<DDGIDirtyBox,8> debug_boxes{};
  std::uint32_t debug_box_cursor{};
  Slang::ComPtr<rhi::IBuffer> debug_box_buffer;
};
bool world_ddgi_initialize(WorldRenderer&);
bool world_ddgi_reconfigure(WorldRenderer&);
bool world_ddgi_update(WorldRenderer&,rhi::ICommandEncoder*);
bool world_ddgi_bind(WorldRenderer&,rhi::IShaderObject*);
void ddgi_schedule(DDGISystem&,const std::array<float,3>& camera);
void ddgi_invalidate(DDGISystem&,const std::array<float,3>& minimum,const std::array<float,3>& maximum,float radius=-1.f);
}
