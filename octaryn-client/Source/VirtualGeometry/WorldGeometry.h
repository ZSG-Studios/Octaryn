#pragma once
#include <slang-rhi.h>
#include <filesystem>
#include <memory>
#include <string>
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
namespace virtual_geometry {
bool world_geometry_requested();
// Monolithic static-map qualification owner. Transparency and RT retain their
// existing owners until their independent geometry integrations are qualified.
class WorldGeometry {
public:
  WorldGeometry();
  ~WorldGeometry();
  bool initialize(WorldRenderer&,const std::filesystem::path& source);
  bool prepare(WorldRenderer&,rhi::ICommandEncoder*,const WorldCamera&);
  bool resolve(WorldRenderer&,rhi::IRenderPassEncoder*);
  bool submitted(rhi::IFence*,std::uint64_t value);
  bool ready() const;
  std::uint64_t gpu_bytes() const;
  const std::string& error() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
}
