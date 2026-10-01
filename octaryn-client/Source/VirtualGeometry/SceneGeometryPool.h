#pragma once
#include "GeometryCache.h"
#include "PageResidency.h"
#include <slang-rhi.h>
#include <memory>
#include <span>

namespace octaryn::client::rendering::virtual_geometry {
class SceneMemoryLedger;
struct SceneGeometryHandle {
  std::uint32_t slot{invalid_id},generation{};
  explicit operator bool() const {return slot!=invalid_id && generation!=0;}
  bool operator==(const SceneGeometryHandle&) const=default;
};
struct SceneGeometryPoolConfig {
  std::uint32_t slots{3072},root_slots{2048},maximum_pages{262144},maximum_assets{8192};
  std::uint32_t feedback_capacity{8192},workers{2},upload_pages{8};
  double upload_ms{1};
  std::shared_ptr<void> scheduler;
};
struct SceneGeometryPoolStats {
  ResidencyStats residency;
  std::uint64_t decoded_pages{},uploaded_pages{},uploaded_bytes{};
  std::uint64_t packed_root_bytes{};
  std::uint32_t assets{},registered_pages{},pinned_pages{},loading_pages{},ready_pages{},uploaded_this_frame{};
};
// One owner thread and graphics queue; all participating draws share its frame timeline.
class SceneGeometryPool {
public:
  SceneGeometryPool();
  ~SceneGeometryPool();
  bool initialize(rhi::IDevice*,std::shared_ptr<SceneMemoryLedger>,const SceneGeometryPoolConfig& = {});
  bool register_asset(const std::filesystem::path&,const GeometryAsset&,SceneGeometryHandle&);
  void release(SceneGeometryHandle);
  // Repeated calls on one encoder add requests/references without repeating uploads.
  bool record(rhi::ICommandEncoder*,SceneGeometryHandle,std::span<const PageRequest>);
  // Idempotent for every participating asset on the same submitted frame.
  bool submitted(rhi::IFence*,std::uint64_t);
  void touch_used(SceneGeometryHandle,std::span<const std::uint32_t>);
  std::vector<GpuPage> page_table(SceneGeometryHandle) const;
  bool roots_ready(SceneGeometryHandle) const;
  bool idle(SceneGeometryHandle) const;
  rhi::IBuffer* buffer() const;
  std::uint64_t gpu_bytes() const;
  SceneGeometryPoolStats stats() const;
  const std::string& error() const;
  bool admission_rejected() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
