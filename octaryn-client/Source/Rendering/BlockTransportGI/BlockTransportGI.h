#pragma once
#include "BlockTransportTypes.h"
#include "BlockTransportAdmission.h"
#include "BlockTransportPolicy.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cstdint>

namespace octaryn::client::rendering {
struct BlockTransportStats {
  std::array<std::uint32_t,12> counters{};
  std::uint64_t measured_frame{},gpu_bytes{},surface_key_bytes{},total_gpu_bytes{},submitted_frames{},resets{},frames_since_reset{},eviction_passes{};
  std::uint64_t admission_sweeps{},admission_complete_frame{},measured_sweeps{},checked_sweeps{};
  std::uint32_t checked_admission_failures{},pinned_rows{},contributor_limit{};
  std::uint64_t contributor_start_frame{UINT64_MAX};
  std::uint32_t admission_faces{},admission_columns{},measured_radiance_epoch{};
  std::uint32_t scheduled_rows{},ready_rows{},selection_start{},selection_occupied{},selection_wraps{};
  std::uint32_t player_visibility_calls{},player_history_visibility_calls{};
  std::uint32_t epoch{},measured_epoch{},light_nodes{},occupied_rows{},initialized_rows{},row_refreshes{};
  bool coverage_valid{},transport_ready{},mandatory_ready{};
};
struct BlockTransportGI {
  Slang::ComPtr<rhi::IComputePipeline> clear,evict,admit,trace,admit_contributors,resolve_contributors,direct,bounce;
  Slang::ComPtr<rhi::IComputePipeline> select_count,select_prefix,select_rows;
  Slang::ComPtr<rhi::IBuffer> surfaces,links,candidates,direct_values,environment,output,counters,light_tree;
  Slang::ComPtr<rhi::IBuffer> lookup_empty_surface,lookup_empty_value;
  Slang::ComPtr<rhi::IBuffer> work_rows,work_blocks,schedule;
  std::array<Slang::ComPtr<rhi::IBuffer>,3> orders;
  std::array<Slang::ComPtr<rhi::IBuffer>,2> readback;
  std::array<bool,2> readback_ready{};
  std::array<std::uint64_t,2> readback_frame{},readback_sweeps{};
  std::array<std::uint32_t,2> readback_epoch{},readback_radiance_epoch{};
  BlockTransportStats stats;
  BlockAdmissionState admission;
  std::array<float,4> coverage_min{},coverage_max{};
  std::uint64_t scene_revision{~0ull},tree_revision{~0ull};
  std::uint64_t pending_scene_revision{},pending_tree_revision{};
  unsigned light_nodes{};
  std::uint32_t epoch{},pending_epoch{};
  std::uint32_t radiance_epoch{},pending_radiance_epoch{};
  std::uint32_t pressure_count{},pending_pressure_count{};
  std::uint64_t player_signature{},pending_player_signature{};
  std::uint32_t player_revision{},pending_player_revision{};
  std::array<float,4> sun{},sky{},pending_sun{},pending_sky{};
  std::array<float,4> lighting_config{},pending_lighting_config{};
  std::array<float,4> cache_origin{},pending_cache_origin{};
  bool active{},pending{},valid{},initialized{},pending_eviction{},inputs_pending{};
};
struct WorldRenderer;
bool prepare_block_transport_gi(WorldRenderer&);
// The selected renderer frame slot must already have completed its fence.
// geometry_ready must include source-to-mesh publication, not only AS readiness.
bool render_block_transport_gi(WorldRenderer&,rhi::ICommandEncoder*,bool geometry_ready);
bool bind_block_transport_lookup(WorldRenderer&,rhi::IShaderObject*);
void commit_block_transport_gi(WorldRenderer&);
}
