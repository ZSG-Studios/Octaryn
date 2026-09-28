#pragma once
#include "RayGeometry.h"
#include <stdexcept>
namespace octaryn::client::rendering::virtual_geometry {
struct RayCopyCluster {
  std::uint32_t cluster{},page{},slot{},generation{},source_vertex{},source_triangle{},vertex_count{},triangle_count{};
  std::uint32_t first_vertex{},first_triangle{},material{},flags{};
};
static_assert(sizeof(RayCopyCluster)==48&&sizeof(RayBatch)==16&&sizeof(RayTriangle)==16);
struct RayGeometry::State {
  struct Build {
    std::shared_ptr<RaySnapshot> scene;
    std::shared_ptr<const RaySnapshot> source_scene;
    Slang::ComPtr<rhi::IBuffer> copies,pages,validation,instances,scratch;
    std::vector<Slang::ComPtr<rhi::IBuffer>> blas_scratch;
    std::vector<rhi::AccelerationStructureBuildInput> blas_inputs;
    std::vector<PageHandle> source_pages;
    Slang::ComPtr<rhi::IFence> fence;std::uint64_t value{},build_bytes{};
    bool recorded{},compaction{};
  };
  struct Use {std::shared_ptr<const RaySnapshot> scene;Slang::ComPtr<rhi::IFence> fence;std::uint64_t value;};
  Slang::ComPtr<rhi::IDevice> device;Slang::ComPtr<rhi::IComputePipeline> expand;
  GeometryAsset asset;SelectionTopology topology;RayGeometryConfig config;
  std::shared_ptr<RaySnapshot> current;std::unique_ptr<Build> pending;
  std::vector<Use> uses;std::vector<PageRequest> feedback;std::string error;
  std::uint64_t generation{};
  void tlas(Build&,rhi::ICommandEncoder*);
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
