#pragma once
#include "SceneSession.h"
#include "SceneAssets.h"
#include "SceneCollisionResidency.h"
#include "../MapWorld/MapRendererInternal.h"
#include "octaryn_native_schedule_runtime.h"
#include <atomic>
#include <map>

namespace octaryn::client::rendering {
struct SceneSession::State {
  struct Entry {scene_geometry::Selection selected;std::shared_ptr<MapRenderer> map;bool published{},ready{};};
  struct Retired {std::shared_ptr<MapRenderer> map;Slang::ComPtr<rhi::IFence> fence;std::uint64_t signal{},reserved{};std::uint32_t part{};};
  struct Job {
    void* task{};
    const SceneAssets* assets{};
    std::uint32_t part{};
    PreparedScenePart prepared;
    std::atomic_bool cancelled{};
    bool success{};
    bool expand{},exact{};
    std::unique_ptr<virtual_geometry::SceneHierarchyDetail> detail;
    std::string error;
    static int execute(void*) noexcept;
  } job;
  SceneAssets assets;
  scene_geometry::Plan plan;
  std::unique_ptr<character_motion::SceneCollisionResidency> collision;
  std::map<std::uint32_t,Entry> entries;
  std::vector<std::uint32_t> cut,pending,removed;
  std::uint32_t replacing{virtual_geometry::invalid_id};
  std::size_t scan{};
  bool startup_committed{};
  std::map<std::uint32_t,std::uint64_t> retry_after;
  std::vector<Retired> retired;
  void* scheduler{};
  Slang::ComPtr<rhi::IFence> last_fence;
  std::uint64_t last_signal{},generation{},frame{},render_frame{},budget{},retired_bytes{};
  std::string error;
  bool loaded{},changed{},trace{},overlay{};
  float pixel_error{1};
  ~State();
  bool collect_retired();
  void retire(std::map<std::uint32_t,Entry>::iterator);
  bool publish(WorldRenderer&);
  bool progress(WorldRenderer&,std::span<const scene_geometry::Selection>);
  bool stage(WorldRenderer&,const WorldCamera&,rhi::ICommandEncoder*);
  bool update_cut(WorldRenderer&,const WorldCamera&);
  bool commit_cut(WorldRenderer&);
};
}
