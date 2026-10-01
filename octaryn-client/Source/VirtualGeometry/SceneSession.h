#pragma once
#include "../WorldStreaming/TileStartupReadiness.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <slang-rhi.h>

namespace octaryn::character_motion {class MeshCollisionScene;}
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
class SceneSession {
public:
  SceneSession();
  ~SceneSession();
  bool load(WorldRenderer&,const std::filesystem::path& catalog,const std::filesystem::path& source,
      std::uint64_t gpu_budget=512ull*1024*1024);
  // Owner-thread publication, one bounded background preparation at a time.
  bool pump(WorldRenderer&,const WorldCamera&,const WorldCamera& actor,rhi::ICommandEncoder*);
  bool submitted(rhi::IFence*,std::uint64_t value);
  void trace_frame(const WorldRenderer&) const;
  bool capture_ready() const;
  TileStartupReadiness startup_readiness() const;
  bool collision_ready(float x,float y,float z,float radius=3);
  std::shared_ptr<character_motion::MeshCollisionScene> collision_scene() const;
  std::uint64_t generation() const;
  const std::string& error() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
