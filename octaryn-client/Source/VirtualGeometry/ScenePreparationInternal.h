#pragma once
#include "ScenePreparation.h"
#include "SceneCatalog.h"
#include "MapSourceReader.h"
#include "GltfTriangleReader.h"
#include <chrono>
#include <memory>
#include <span>

namespace octaryn::client::rendering::virtual_geometry {
inline std::filesystem::path preparation_path(const std::string& value) {
  return std::filesystem::path(reinterpret_cast<const char8_t*>(value.c_str()));
}
struct ScenePreparationWork {
  std::filesystem::path path;
  SceneCatalog catalog;
  const std::atomic_bool* cancel{};
  ScenePreparationNotify notify;
  ScenePreparationResult result;
  std::vector<std::filesystem::file_time_type> resource_times;
  std::unique_ptr<MapSourceReader> reader;
  std::unique_ptr<assets::GltfTriangleReader> bounds_reader;
  std::chrono::steady_clock::time_point saved=std::chrono::steady_clock::now();
  bool modified{};
  void check() const;
  void verify_resources();
  void checkpoint(bool force=false);
  void report(ScenePreparationStage,std::uint64_t completed,std::uint64_t requested);
  void bounds(std::span<const std::uint32_t> parts);
  void cook(std::span<const std::uint32_t> parts);
  void finish();
};
}
