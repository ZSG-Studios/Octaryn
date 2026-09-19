#pragma once
#include "WorldRayTracing.h"
#include "RayTracingTiming.h"
#include "RayPrepareDiagnostics.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <array>
#include <map>
namespace octaryn::client::rendering {
namespace world_ray {
using Coord=std::pair<std::int32_t,std::int32_t>;
struct Record {
  std::uint64_t faces{},fluids{};
  std::uint32_t fluid_base{},face_count{},reserved[2]{};
};
static_assert(sizeof(Record)==32);
struct Column {
  Slang::ComPtr<rhi::IBuffer> faces,fluids;
  Slang::ComPtr<rhi::IAccelerationStructure> blas;
  Record record;
  std::uint32_t refits{};
  bool matches(const WorldColumnGpu& c) const {
    return faces.get()==c.faces.get() && fluids.get()==c.fluids.get() &&
      record.face_count==c.face_count && record.fluid_base==c.pass_counts[0]+c.pass_counts[1]+c.pass_counts[2];
  }
};
struct Snapshot {
  Slang::ComPtr<rhi::IAccelerationStructure> tlas;
  Slang::ComPtr<rhi::IBuffer> records;
  std::vector<std::shared_ptr<Column>> columns;
  std::uint64_t generation{};
};
struct Frame {
  RayTracingTiming timing;
  std::shared_ptr<Snapshot> snapshot;
  std::shared_ptr<Snapshot> update_source;
  Slang::ComPtr<rhi::IBuffer> instances,scratch,dummy_bounds,dummy_scratch;
};
struct BuildJob {
  RayTracingTiming timing;
  Slang::ComPtr<rhi::ICommandBuffer> submission;
  Slang::ComPtr<rhi::IBuffer> bounds,scratch;
  std::shared_ptr<Column> pending,refit_source;
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
  rhi::BufferDesc desc{};desc.size=std::max<std::uint64_t>(bytes,stride);desc.elementSize=stride;
  desc.usage=usage;desc.defaultState=initial;
  const auto status=r.device->createBuffer(desc,nullptr,result.writeRef());
  return diagnostic?diagnostic->check(step,status):world_rhi_ok(status);
}
inline bool descriptor(rhi::IBuffer* buffer,std::uint64_t& value,RayPrepareDiagnostics& diagnostic,const char* step) {
  rhi::DescriptorHandle handle{};
  if(!diagnostic.require(step,buffer!=nullptr))return false;
  diagnostic.bytes=buffer->getDesc().size;
  if(!diagnostic.check(step,buffer->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,
      rhi::Format::Undefined,rhi::kEntireBuffer,&handle)) ||
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
  bool available{},bytes_dirty{true};
  unsigned active_slot{};
  Slang::ComPtr<rhi::IComputePipeline> bounds_pipeline;
  Slang::ComPtr<rhi::IAccelerationStructure> dummy;
  Slang::ComPtr<rhi::IFence> fence;
  std::array<BuildJob,4> jobs;
  unsigned build_budget{2},face_budget{262144};
  std::vector<std::pair<double,Coord>> candidates;
  std::uint64_t signal{},generation{1};
  std::map<Coord,std::shared_ptr<Column>> columns;
  std::map<Coord,std::shared_ptr<Column>> changed;
  // Per-pass face counts of the last completed BLAS per column (window-bounded).
  // A rebuild with identical counts is sprite/topology churn, not new occluders.
  std::map<Coord,std::array<std::uint32_t,5>> built_pass_counts;
  std::shared_ptr<Snapshot> current;
  std::array<Frame,2> frames;
  WorldRayTracingStats stats;

  void refresh_bytes(const WorldRenderer&);
  bool poll(WorldRenderer&);
  bool start(WorldRenderer&,Coord,const WorldColumnGpu&);
  bool snapshot(WorldRenderer&,rhi::ICommandEncoder*,Frame&);
  bool empty_blas(WorldRenderer&,rhi::ICommandEncoder*,Frame&);
};
}
