#pragma once
#include <memory>
#include <filesystem>
#include <cstdint>
namespace octaryn::character_motion {class MeshCollisionScene;}
namespace octaryn::client::app {
class SceneBodyMirror {
public:
  SceneBodyMirror();
  ~SceneBodyMirror();
  bool load(std::shared_ptr<character_motion::MeshCollisionScene>,const std::filesystem::path& source);
  bool pose(uint64_t,const float* position,const float* rotation,const float* velocity,bool removed);
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
