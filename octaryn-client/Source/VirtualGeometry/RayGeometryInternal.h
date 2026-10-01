#pragma once
#include "RayGeometry.h"
#include "SceneRayScheduler.h"
#include <stdexcept>
namespace octaryn::client::rendering::virtual_geometry {
struct RayCopyCluster {
  std::uint32_t cluster{},page{},slot{},generation{},source_vertex{},source_triangle{},vertex_count{},triangle_count{};
  std::uint32_t first_vertex{},first_triangle{},material{},flags{};
};
static_assert(sizeof(RayCopyCluster)==48&&sizeof(RayBatch)==16&&sizeof(RayTriangle)==16);
struct RayGeometry::State {
  struct Build {
    enum class Phase {Complete,Build,Compact};
    std::shared_ptr<RaySnapshot> scene;
    std::shared_ptr<const RaySnapshot> source_scene;
    Slang::ComPtr<rhi::IBuffer> copies,pages,validation,instances,scratch;
    std::vector<rhi::AccelerationStructureBuildInput> blas_inputs;
    std::vector<rhi::AccelerationStructureSizes> blas_sizes;
    std::vector<Slang::ComPtr<rhi::IAccelerationStructure>> compact_sources;
    std::vector<PageHandle> source_pages;
    Slang::ComPtr<rhi::IFence> fence;std::uint64_t value{},build_bytes{};
    std::uint64_t tlas_reserve{},compact_source_bytes{};
    std::uint32_t first_batch{},batch_count{},next_batch{};
    Phase phase{Phase::Complete};bool staged{},ready{};
    bool recorded{},compaction{};
  };
  struct Use {std::shared_ptr<const RaySnapshot> scene;Slang::ComPtr<rhi::IFence> fence;std::uint64_t value;};
  Slang::ComPtr<rhi::IDevice> device;Slang::ComPtr<rhi::IComputePipeline> expand;
  GeometryAsset asset;SelectionTopology topology;RayGeometryConfig config;
  RayGeometryBudget budget;
  std::shared_ptr<RaySnapshot> current;std::unique_ptr<Build> pending;
  std::vector<std::weak_ptr<const RaySnapshot>> retired;
  std::vector<Use> uses;std::vector<PageRequest> feedback;std::string error;
  std::uint64_t generation{};
  void tlas(Build&,rhi::ICommandEncoder*);
  void stage_build(Build&,rhi::ICommandEncoder*);
  void stage_compact(Build&,rhi::ICommandEncoder*);
  std::uint64_t live_bytes() const;
};
namespace ray_geometry {
inline void check(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
inline void checked(SlangResult result,const char* reason) {check(SLANG_SUCCEEDED(result),reason);}
Slang::ComPtr<rhi::IBuffer> buffer(rhi::IDevice*,std::uint64_t,std::uint32_t,rhi::BufferUsage,const void* initial=nullptr);
Slang::ComPtr<rhi::IAccelerationStructure> acceleration(rhi::IDevice*,rhi::AccelerationStructureKind,std::uint64_t);
bool completed(rhi::IFence*,std::uint64_t);
std::vector<std::uint32_t> spatial_order(const GeometryAsset&,std::vector<std::uint32_t>);
}
}
