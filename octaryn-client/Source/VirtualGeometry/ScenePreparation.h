#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace octaryn::client::rendering::virtual_geometry {
enum class ScenePreparationStage { Inspect, Bounds, Geometry, Ready, Canceled, Failed, Layout, Spawn };
struct ScenePreparationProgress {
  ScenePreparationStage stage{ScenePreparationStage::Inspect};
  std::uint64_t completed{},requested{},total_parts{},prepared_bounds{},cooked_parts{};
};
using ScenePreparationNotify=std::function<void(const ScenePreparationProgress&)>;
struct ScenePreparationRequest {
  std::filesystem::path source,catalog;
  std::array<float,3> camera{},actor{};
  float render_radius{128},collision_radius{3};
  std::uint64_t gpu_budget_bytes{512ull*1024*1024},collision_budget_bytes{512ull*1024*1024};
};
struct ScenePreparationResult {
  std::uint64_t total_parts{},prepared_bounds{},cooked_parts{},render_parts{},collision_pairs{};
  std::uint64_t render_bytes{},collision_bytes{};
  std::vector<std::uint32_t> wanted_parts,pending_bounds,pending_cooks;
  bool neighborhood_ready{},full_scene_ready{},canceled{};
};
// Runs on a caller-owned worker. Cancellation checkpoints occur between bounded cook parts.
bool prepare_scene_neighborhood(const ScenePreparationRequest&,ScenePreparationResult&,std::string& error,
    const std::atomic_bool* cancel=nullptr,ScenePreparationNotify notify={});
enum class ScenePreparationMode { Bounds, Geometry };
bool prepare_scene_range(const std::filesystem::path& catalog,std::uint64_t first,std::uint64_t count,
    ScenePreparationMode,ScenePreparationResult&,std::string& error,
    const std::atomic_bool* cancel=nullptr,ScenePreparationNotify notify={});
bool qualify_scene_spawn(const std::filesystem::path& catalog,const std::filesystem::path& source,
    const std::array<float,3>& hint,std::array<float,3>& output,std::string& error,const std::atomic_bool* cancel=nullptr);
}
