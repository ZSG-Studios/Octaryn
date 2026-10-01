#pragma once
#include "TileStartupReadiness.h"
#include <slang-rhi.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>
namespace octaryn::character_motion {class MeshCollisionScene;}
struct camera;
namespace octaryn::client::rendering {
struct MapRenderer;
struct WorldCamera;
struct MapRaySubmitScope;
struct TileStreamBudget {
  float load_radius{128},keep_radius{160},actor_radius{24};
  std::uint64_t gpu_bytes{2ull*1024*1024*1024},upload_bytes{2ull*1024*1024};
  std::uint64_t preparation_bytes{512ull*1024*1024};
  double upload_ms{.2};
  double readiness_ms{2000};
};
struct TileStreamStats {
  unsigned wanted{},resident{},preparing{},uploading{},cancelled{},evicted{},published{};
  std::uint64_t resident_bytes{},reserved_bytes{},retired_bytes{},generation{};
  std::uint64_t texture_bytes{};
  std::uint64_t preparation_reserved_bytes{};
  unsigned deadline_misses{};
};
// Owner-thread publication; CPU parsing and Box3D BVHs use two bounded jobs.
class TileSession {
public:
  TileSession();
  ~TileSession();
  TileSession(const TileSession&)=delete;
  TileSession& operator=(const TileSession&)=delete;
  bool load(const std::filesystem::path&,rhi::IDevice*,rhi::ICommandQueue*,TileStreamBudget);
  bool pump(const WorldCamera&,const WorldCamera& actor,rhi::ICommandEncoder*,bool ray_required,
            std::vector<std::shared_ptr<MapRenderer>>& published,const MapRaySubmitScope* profile=nullptr);
  // Supply the fence value of the frame containing the recorded upload commands.
  void submitted(rhi::IFence*,std::uint64_t value);
  // Only caller-encoder writes; BLAS lifecycle owns its submissions and fences.
  bool commands_recorded() const;
  bool capture_ready() const;
  TileStartupReadiness startup_readiness(const ::camera&) const;
  bool collision_ready(float x,float y,float z,float radius=3) const;
  std::shared_ptr<character_motion::MeshCollisionScene> collision_scene() const;
  TileStreamStats stats() const;
  const char* error() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
