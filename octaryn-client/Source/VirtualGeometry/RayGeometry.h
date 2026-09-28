#pragma once
#include "Selection.h"
#include <slang-rhi.h>
#include <memory>
namespace octaryn::client::rendering::virtual_geometry {
struct RayGeometryConfig {
  std::uint32_t clusters_per_blas{128},maximum_clusters{8192},feedback_capacity{4096};
  float error_pixels{4};
  std::uint64_t maximum_build_bytes{512ull<<20},maximum_resident_bytes{1024ull<<20};
};
struct RayBatch {std::uint32_t first_triangle{},triangle_count{},first_cluster{},cluster_count{};};
struct RayTriangle {std::uint32_t cluster{},local_triangle{},material{},flags{};};
struct RaySnapshot {
  std::uint64_t generation{},bytes{};
  std::string source_hash;
  std::vector<std::uint32_t> clusters;
  std::vector<RayBatch> batches;
  // BLAS primitive index + batches[TLAS instance ID].first_triangle indexes triangle_records.
  Slang::ComPtr<rhi::IBuffer> vertices,indices,triangle_records,batch_records;
  std::vector<Slang::ComPtr<rhi::IAccelerationStructure>> blas;
  Slang::ComPtr<rhi::IAccelerationStructure> tlas;
  Slang::ComPtr<rhi::IQueryPool> compact_sizes;
};
class RayGeometry {
public:
  RayGeometry();~RayGeometry();
  RayGeometry(const RayGeometry&)=delete;RayGeometry& operator=(const RayGeometry&)=delete;
  bool initialize(rhi::IDevice*,const GeometryAsset&,const char* shader_path,const RayGeometryConfig& config={});
  // Source page handles must stay referenced until the submitted build fence completes.
  // Selection includes every root; raster frustum/Hi-Z never determines ray residency.
  bool record(rhi::ICommandEncoder*,rhi::IBuffer* page_pool,std::span<const GpuPage>,const SelectionView&);
  // False with an empty error means no beneficial compaction is available.
  bool record_compaction(rhi::ICommandEncoder*);
  // Submit even a partially recorded failure before calling this, or discard its encoder
  // then cancel_unsubmitted(). Never destroy the owner while its fences are incomplete.
  bool submitted(rhi::IFence*,std::uint64_t value);
  void cancel_unsubmitted();
  bool poll();
  std::shared_ptr<const RaySnapshot> snapshot() const;
  // Register every ray-consumer submission; resources remain retained through its fence.
  bool reference(std::shared_ptr<const RaySnapshot>,rhi::IFence*,std::uint64_t value);
  std::span<const PageHandle> pending_pages() const;
  std::span<const PageRequest> requests() const;
  const std::string& error() const;
private:
  struct State;std::unique_ptr<State> state_;
};
}
