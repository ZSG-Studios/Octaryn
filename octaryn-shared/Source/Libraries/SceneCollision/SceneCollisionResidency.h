#pragma once
#include "MeshCollisionScene.h"
#include <filesystem>
#include <atomic>
#include <array>
#include <memory>
#include <string>

namespace octaryn::character_motion {
struct SceneCollisionStats {
  std::uint32_t resident{},preparing{},failed{};
  std::uint64_t resident_bytes{},reserved_bytes{},budget_bytes{},loads{},evictions{},waits{};
};
class SceneCollisionResidency {
public:
  SceneCollisionResidency();
  ~SceneCollisionResidency();
  bool load(const std::filesystem::path& catalog,const std::filesystem::path& source,
      const std::filesystem::path& scratch,std::uint64_t budget_bytes=512ull*1024*1024,const std::atomic_bool* cancel=nullptr);
  // Owner-thread pump. A missing protected part returns false without advancing movement.
  bool ready(float x,float y,float z,float radius);
  bool ready_bounds(const std::array<float,6>&);
  std::shared_ptr<MeshCollisionScene> scene() const;
  SceneCollisionStats stats() const;
  std::uint64_t triangle_count() const;
  const std::string& error() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
