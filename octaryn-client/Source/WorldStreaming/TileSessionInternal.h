#pragma once
#include "TileSession.h"
#include "TileSet.h"
#include "MapAssetBuild.h"
#include "MapRenderer.h"
#include "MeshCollisionScene.h"
#include "octaryn_native_schedule_runtime.h"
#include <array>
#include <slang-com-ptr.h>
#include <atomic>
#include <string>
#include <chrono>
namespace octaryn::client::rendering {
struct TileSession::State {
  enum class Phase {Absent,Loading,Prepared,Uploading,Ray,Ready};
  struct Entry {
    Phase phase{Phase::Absent};
    bool wanted{},keep{};
    bool ray_budget_checked{};
    bool ray_active{};
    bool cancelled_upload{};
    bool deadline_reported{};
    std::chrono::steady_clock::time_point requested_at{};
    float priority{};
    std::uint64_t reserved{};
    std::unique_ptr<PreparedMapAsset> prepared;
    std::unique_ptr<character_motion::PreparedCollisionTile> collision;
    MapRendererBuild* builder{};
    std::shared_ptr<MapRenderer> map;
  };
  struct Job {
    void* task{};
    unsigned tile{};
    std::filesystem::path source;
    std::filesystem::path texture_cache;
    std::shared_ptr<const MapTextureReuseIndex> texture_reuse;
    std::atomic_bool cancelled{};
    std::unique_ptr<PreparedMapAsset> prepared;
    std::unique_ptr<character_motion::PreparedCollisionTile> collision;
    std::string error;
    bool success{};
    static int execute(void*) noexcept;
  };
  app::TileSet tiles;
  TileStreamBudget budget;
  TileStreamStats statistics;
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::ICommandQueue> queue;
  Slang::ComPtr<rhi::IBuffer> size_query_address;
  std::shared_ptr<MapTexturePool> textures;
  std::shared_ptr<character_motion::MeshCollisionScene> collision;
  std::vector<Entry> entries;
  std::vector<unsigned> priority;
  std::array<Job,2> jobs;
  void* scheduler{};
  struct Retired {
    std::shared_ptr<MapRenderer> map;Slang::ComPtr<rhi::IFence> fence;
    std::uint64_t signal{},reserved{};
  };
  std::vector<Retired> retired;
  unsigned retired_ray_pending{};
  Slang::ComPtr<rhi::IFence> last_fence;
  std::uint64_t last_signal{};
  MapRendererBuild* recorded{};
  std::string error;
  std::uint64_t frame{};
  bool changed{};
  bool commands_recorded{};
  std::uint64_t memory_sample{},admitted_since_sample{};
  bool memory_pause_logged{};
  bool texture_reuse_enabled{true};
  std::uint64_t texture_reuses{},avoided_dds_bytes{};
  std::uint64_t ray_polls{},ray_wait_fences{},ray_wait_allocations{},ray_operations{},ray_submissions{};
  unsigned ray_capacity{4};
  ~State();
  bool poll_jobs();
  bool start_jobs();
  bool progress(rhi::ICommandEncoder*,bool ray_required,const MapRaySubmitScope*);
  bool progress_rays(bool ray_required,const MapRaySubmitScope*);
  bool reserve_ray(unsigned);
  bool publish_ray(unsigned);
  void decide(const WorldCamera&,const WorldCamera&);
  void refresh();
  void evict(unsigned);
  bool collect_retired();
};
}
