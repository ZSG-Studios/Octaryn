#pragma once
#include "GeometryTransform.h"
#include "SceneRasterBudget.h"
#include "SelectionGpu.h"
#include <slang-rhi.h>
#include <memory>
#include <span>

namespace octaryn::client::rendering::virtual_geometry {
class SceneMemoryLedger;
struct SceneRasterAsset {
  rhi::IBuffer* clusters{};
  std::uint32_t cluster_count{},page_count{},material_base{};
  std::span<const GeometryTransform> instances;
};
struct SceneRasterFrame {
  rhi::IBuffer* clusters{};
  rhi::IBuffer* pages{};
  rhi::IBuffer* instances{};
  rhi::IBuffer* draws{};
  rhi::IBuffer* selected{};
  rhi::IBuffer* counters{};
  rhi::IBuffer* dispatch{};
  std::uint32_t generation{},capacity{},slot{};
};
// Two frame banks retain global ranges and draw identities until their actual consumer fence.
class SceneRasterTables {
public:
  SceneRasterTables();
  ~SceneRasterTables();
  bool initialize(rhi::IDevice*,std::shared_ptr<SceneMemoryLedger>,const char* shader_directory);
  static std::uint64_t required_bytes(SceneRasterCapacity);
  static bool capacity(std::span<const SceneRasterAsset>,SceneRasterCapacity&);
  bool reserve(SceneRasterCapacity);
  bool begin(rhi::ICommandEncoder*,std::uint32_t slot,std::span<const SceneRasterAsset>,SceneRasterFrame&);
  bool append(rhi::ICommandEncoder*,std::uint32_t asset,const SelectionGpuFrame&);
  bool append_roots(rhi::ICommandEncoder*,std::uint32_t asset,std::span<const GpuPage>);
  bool finish(rhi::ICommandEncoder*);
  bool submitted(rhi::IFence*,std::uint64_t);
  std::uint64_t gpu_bytes() const;
  const std::string& error() const;
  bool admission_rejected() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
