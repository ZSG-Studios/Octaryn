#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <utility>
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
  std::uint32_t pending_probes{};
  float oldest_update_seconds{};
  std::uint64_t bytes{};
};
struct DDGIDirtyBox {
  std::uint64_t frame{};
  std::array<float,3> minimum{},maximum{};
};
struct DDGISystem {
  DDGIConfig config;
  DDGIConfig base_config;
  std::unique_ptr<DDGISystem> fine_volume;
  DDGIStats stats;
  Slang::ComPtr<rhi::IBuffer> controls,probes,irradiance,distance,rays,variability;
  std::array<Slang::ComPtr<rhi::IBuffer>,2> selections;
  std::array<Slang::ComPtr<rhi::IBuffer>,2> history_intervals;
  Slang::ComPtr<rhi::IComputePipeline> trace,update,seed;
  Slang::ComPtr<rhi::IRenderPipeline> debug;
  std::vector<DDGIControl> control_data;
  std::vector<std::uint64_t> last_updates;
  std::vector<double> last_update_times;
  std::vector<float> selected_intervals;
  double time_seconds{},light_consumed_seconds{},frame_seconds{1./60},budget_credit{};
  std::chrono::steady_clock::time_point update_clock{};
  std::vector<bool> dirty;
  std::vector<std::uint32_t> selected;
  std::vector<std::uint8_t> occupancy;
  std::array<std::int32_t,3> origin{};
  std::array<float,3> fade_origin{};
  float env_spacing{8};
  std::array<int,3> ignore_voxel{std::numeric_limits<int>::max(),std::numeric_limits<int>::max(),
    std::numeric_limits<int>::max()};
  // Topmost occupied voxel height per (x,z) column, rebuilt when the scene
  // revision changes. Exact open-sky test: probes above it see sky; the rest
  // must be traced or sealed caves are classed as open air and keep seeded
  // daylight forever.
  std::map<std::pair<std::int32_t,std::int32_t>,std::int32_t> sky_tops;
  std::uint64_t sky_tops_revision{~0ull};
  std::array<std::int32_t,3> classified_origin{std::numeric_limits<std::int32_t>::max(),
    std::numeric_limits<std::int32_t>::max(),std::numeric_limits<std::int32_t>::max()};
  std::array<int,3> classified_ignore_voxel{std::numeric_limits<int>::max(),std::numeric_limits<int>::max(),
    std::numeric_limits<int>::max()};
  std::uint64_t classified_revision{~0ull};
  bool classified_ignore{};
  bool available{},initialized{},controls_dirty{true},cell_centered{},seed_needed{true};
  bool ignore_active{},ignore_held{};
  std::uint32_t ignore_released{};
  std::uint64_t frame{},scene_revision{},light_revision{},ignore_revision{};
  // Last published light influence bounds (position xyz, reach w) so removed or
  // moved lights also wake exactly the region they used to touch.
  std::vector<std::array<float,4>> light_bounds;
  unsigned dispatch_capacity{};
  unsigned burst_frames{};
  // Recent invalidation regions for the dirty-region debug view (mode 27).
  std::array<DDGIDirtyBox,8> debug_boxes{};
  std::uint32_t debug_box_cursor{};
  Slang::ComPtr<rhi::IBuffer> debug_box_buffer;
};
bool world_ddgi_initialize(WorldRenderer&);
bool world_ddgi_reconfigure(WorldRenderer&);
bool world_ddgi_update(WorldRenderer&,rhi::ICommandEncoder*);
bool world_ddgi_bind(WorldRenderer&,rhi::IShaderObject*);
void ddgi_scroll(DDGISystem&,const std::array<float,3>& camera);
void ddgi_schedule(DDGISystem&,const std::array<float,3>& camera);
// lights=true marks a light-only wake (padding.z): hard=true snaps and drops
// the probes from the cage until retraced (removals), hard=false blends them
// smoothly at background tier (additions, flicker). Geometry wakes snap.
void ddgi_invalidate(DDGISystem&,const std::array<float,3>& minimum,const std::array<float,3>& maximum,float radius=-1.f,bool lights=false,bool hard=false);
}
