#pragma once
#include "SceneCollisionResidency.h"
#include "SceneCollisionCatalog.h"
#include "GltfTriangleReader.h"
#include "octaryn_native_schedule_runtime.h"
#include <chrono>
#include <map>
#include <atomic>

namespace octaryn::character_motion {
struct SceneCollisionResidency::State {
  using Clock=std::chrono::steady_clock;
  struct Entry {
    Clock::time_point required{},wanted{};
    std::uint64_t bytes{},triangles{};
    bool resident{};
  };
  struct Job {
    State* owner{};void* task{};std::uint64_t key{},bytes{},triangles{};
    std::uint32_t part{},instance{};bool success{};
    std::unique_ptr<PreparedCollisionTile> prepared;
    std::string error;
    static int execute(void*);
  } job;
  SceneCollisionCatalog catalog;
  scene_geometry::ResidencyIndex index;
  assets::GltfTriangleReader reader;
  std::atomic_bool canceled{};
  void* scheduler{};
  std::map<std::uint64_t,Entry> entries;
  std::vector<std::uint64_t> required;
  std::vector<scene_geometry::Selection> current;
  std::shared_ptr<MeshCollisionScene> collision=std::make_shared<MeshCollisionScene>();
  SceneCollisionStats statistics;
  std::string error;
  ~State();
  bool ready(const scene_geometry::Query&);
  bool poll();
  bool start(std::uint64_t key);
  void refresh();
  std::uint64_t reservation(std::uint32_t part) const;
};
}
