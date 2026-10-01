#pragma once
#include "WorldRayTracing.h"
#include "WorldRayBuildBudget.h"
#include "WorldRayAllocator.h"
#include "WorldRayCapacity.h"
#include "DeviceMemory.h"
#include "RayTracingTiming.h"
#include "RayPrepareDiagnostics.h"
#include "WorldRendererInternal.h"
#include "../../VirtualGeometry/WorldGeometryRay.h"
#include "../../VirtualGeometry/SceneMemoryLedger.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <array>
#include <bit>
#include <map>
namespace octaryn::client::rendering {
namespace world_ray {
struct SceneAdmission;
using Coord=std::pair<std::int32_t,std::int32_t>;
struct Record {
  std::uint64_t faces{};
  std::uint32_t face_count{},reserved[2]{};
};
static_assert(sizeof(Record)==24);
struct Column {
  Slang::ComPtr<rhi::IBuffer> faces;
  Slang::ComPtr<rhi::IAccelerationStructure> blas;
  Record record;
  std::uint32_t refits{};
};
struct Snapshot {
  std::shared_ptr<virtual_geometry::SceneMemoryLease> scene_allocation;
  std::shared_ptr<DeviceMemoryReservation> capacity_reservation;
  Slang::ComPtr<rhi::IAccelerationStructure> tlas;
  std::vector<Slang::ComPtr<rhi::IAccelerationStructure>> map_blas;
  std::vector<std::shared_ptr<MapRenderer>> maps;
  std::vector<std::shared_ptr<const virtual_geometry::RaySnapshot>> geometry_snapshots;
  std::vector<std::uint64_t> map_instance_revisions;
  std::vector<std::shared_ptr<MapRenderer>> item_assets;
  std::uint32_t static_instance_count{};
  std::uint64_t item_revision{};
  Slang::ComPtr<rhi::IBuffer> map_records;
  Slang::ComPtr<rhi::IBuffer> records;
  std::vector<std::shared_ptr<Column>> columns;
  std::uint64_t generation{};
};
struct Frame {
  std::shared_ptr<virtual_geometry::SceneMemoryLease> scene_allocation;
  std::shared_ptr<DeviceMemoryReservation> capacity_reservation;
  RayTracingTiming timing;
  std::shared_ptr<Snapshot> snapshot;
  std::shared_ptr<Snapshot> update_source;
  Slang::ComPtr<rhi::IBuffer> instances,scratch,dummy_bounds,dummy_scratch;
};
inline void clear_snapshot_owners(Snapshot& scene) {
  scene.columns.clear();scene.map_blas.clear();scene.maps.clear();scene.geometry_snapshots.clear();
  scene.map_instance_revisions.clear();
  scene.item_assets.clear();scene.static_instance_count=0;scene.generation=0;
}
struct BuildJob {
  RayTracingTiming timing;
  Slang::ComPtr<rhi::ICommandBuffer> submission;
  Slang::ComPtr<rhi::IBuffer> bounds,scratch;
  std::shared_ptr<Column> pending,refit_source;
  std::shared_ptr<WorldRayAllocation> allocation;
  Coord coordinate{};
  std::uint64_t signal{};
  bool cancelled{};
};
inline bool buffer(WorldRenderer& r,std::uint64_t bytes,unsigned stride,rhi::BufferUsage usage,
    rhi::ResourceState initial,Slang::ComPtr<rhi::IBuffer>& result,
    RayPrepareDiagnostics* diagnostic=nullptr,const char* step="buffer_create") {
  if(diagnostic)diagnostic->bytes=std::max<std::uint64_t>(bytes,stride);
  if(result && result->getDesc().size>=std::max<std::uint64_t>(bytes,stride))
    return diagnostic?diagnostic->require(step,true):true;
  // Scratch and fenced frame buffers grow by capacity, not by each added
  // column. Keep the allocation an exact multiple of its structured stride.
  const auto elements=(std::max<std::uint64_t>(bytes,stride)+stride-1)/stride;
  rhi::BufferDesc desc{};desc.size=std::bit_ceil(elements)*stride;desc.elementSize=stride;
  desc.usage=usage;desc.defaultState=initial;
  const auto status=r.device->createBuffer(desc,nullptr,result.writeRef());
  return diagnostic?diagnostic->check(step,status):world_rhi_ok(status);
}
inline bool descriptor(rhi::IBuffer* buffer,std::uint64_t& value,RayPrepareDiagnostics& diagnostic,const char* step,
    rhi::BufferRange range=rhi::kEntireBuffer) {
  rhi::DescriptorHandle handle{};
  if(!diagnostic.require(step,buffer!=nullptr))return false;
  diagnostic.bytes=buffer->getDesc().size;
  if(!diagnostic.check(step,buffer->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,
      rhi::Format::Undefined,range,&handle)) ||
      !diagnostic.require("descriptor_type",handle.type==rhi::DescriptorHandleType::Buffer))return false;
  value=handle.value;return true;
}
inline bool bind_buffer(rhi::IShaderObject* root,const char* name,rhi::IBuffer* value,RayPrepareDiagnostics* diagnostic=nullptr) {
  auto cursor=rhi::ShaderCursor(root)[name];
  if(!cursor.isValid())return true;
  const auto result=cursor.setBinding(rhi::Binding(value));
  return diagnostic?diagnostic->check(name,result):world_rhi_ok(result);
}
}
using namespace world_ray;
struct WorldRayTracing::State {
  std::shared_ptr<virtual_geometry::SceneMemoryLease> scene_fixed_allocation;
  std::shared_ptr<world_ray::SceneAdmission> scene_admission;
  bool available{},bytes_dirty{true};
  unsigned active_slot{};
  Slang::ComPtr<rhi::IComputePipeline> bounds_pipeline;
  Slang::ComPtr<rhi::IAccelerationStructure> dummy;
  Slang::ComPtr<rhi::IFence> fence;
  std::array<BuildJob,BuildJobCapacity> jobs;
  unsigned build_budget{BuildJobCapacity},face_budget{262144};
  // Reset at frame head only; cap slices share the remaining submission budget.
  FrameBuildBudget submission_budget;
  std::vector<std::pair<double,Coord>> candidates;
  std::uint64_t signal{},generation{1};
  std::map<Coord,std::shared_ptr<Column>> columns;
  std::map<Coord,std::shared_ptr<Column>> changed;
  // Per-pass face counts of the last completed BLAS per column (window-bounded).
  // A rebuild with identical counts is sprite/topology churn, not new occluders.
  std::map<Coord,std::array<std::uint32_t,5>> built_pass_counts;
  std::shared_ptr<Snapshot> current;
  // One completed, exclusive snapshot retains capacity without retaining geometry.
  std::shared_ptr<Snapshot> spare;
  std::array<std::shared_ptr<Snapshot>,SceneSnapshotCount> snapshot_pool;
  unsigned prewarm_items{},prewarm_maps{},reported_item_growth{};
  std::array<Frame,SceneFrameCount> frames;
  std::vector<Record> snapshot_records;
  std::vector<MapRayGeometry> snapshot_map_records;
  std::vector<rhi::AccelerationStructureInstanceDescGeneric> snapshot_instances;
  std::vector<std::uint8_t> snapshot_native;
  std::vector<const Column*> accounting_columns;
  std::vector<const Snapshot*> accounting_snapshots;
  std::vector<rhi::IBuffer*> accounting_meshes;
  WorldRayTracingStats stats;
  // Destroy/join before the jobs and their reusable buffer owners.
  std::unique_ptr<WorldRayAllocator> allocator;

  void refresh_bytes(const WorldRenderer&);
  bool poll(WorldRenderer&);
  bool progress_allocations(WorldRenderer&,std::uint64_t budget_ns);
  bool submit(WorldRenderer&,BuildJob&);
  bool snapshot(WorldRenderer&,rhi::ICommandEncoder*,Frame&,std::shared_ptr<Snapshot> reusable);
  bool empty_blas(WorldRenderer&,rhi::ICommandEncoder*,Frame&);
};
}
