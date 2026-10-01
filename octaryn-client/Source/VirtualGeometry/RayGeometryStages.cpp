#include "RayGeometryInternal.h"
#include <algorithm>
namespace octaryn::client::rendering::virtual_geometry {
using namespace ray_geometry;
void RayGeometry::State::stage_build(Build& pending,rhi::ICommandEncoder* commands) {
  auto& scene=*pending.scene;
  const auto live=live_bytes();
  auto limit=std::min(config.maximum_build_bytes,
      live<config.maximum_resident_bytes?config.maximum_resident_bytes-live:0);
  if(scene.allocation) {
    limit=std::min(limit,scene.allocation->bytes()+budget.scratch_bytes);
    scene.allocation->phase(SceneMemoryPhase::Pending);
  }
  pending.first_batch=pending.next_batch;pending.batch_count=0;
  std::uint64_t remaining{};
  for(std::size_t i=pending.next_batch;i<pending.blas_sizes.size();++i)
    remaining+=pending.blas_sizes[i].accelerationStructureSize;
  const bool final_fits=pending.build_bytes+pending.tlas_reserve+remaining<=limit;
  std::uint64_t group_bytes{};
  while(pending.next_batch<scene.batches.size() && pending.batch_count<32) {
    const auto bytes=pending.blas_sizes[pending.next_batch].accelerationStructureSize;
    const auto required=pending.build_bytes+pending.tlas_reserve+2*(group_bytes+bytes);
    // Retain uncompressed sources until the compact-copy fence completes.
    if(required>limit && !final_fits)break;
    group_bytes+=bytes;++pending.next_batch;++pending.batch_count;
  }
  budget.limited=pending.batch_count==0;
  budget.resident_limited=budget.limited && limit<config.maximum_build_bytes;
  if(budget.limited)throw std::runtime_error("ray staged compaction cannot fit the next complete batch: completed="+
      std::to_string(pending.next_batch)+"/"+std::to_string(scene.batches.size())+" allocated="+
      std::to_string(pending.build_bytes)+" remaining_blas="+std::to_string(remaining)+" limit="+std::to_string(limit));
  commands->setBufferState(scene.vertices,rhi::ResourceState::AccelerationStructureBuildInput);
  commands->setBufferState(scene.indices,rhi::ResourceState::AccelerationStructureBuildInput);
  for(std::uint32_t i=pending.first_batch;i<pending.next_batch;++i) {
    const auto bytes=pending.blas_sizes[i].accelerationStructureSize;
    scene.blas[i]=acceleration(device,rhi::AccelerationStructureKind::BottomLevel,bytes);
    scene.bytes+=bytes;pending.build_bytes+=bytes;
    rhi::AccelerationStructureBuildDesc desc{};desc.inputs=&pending.blas_inputs[i];desc.inputCount=1;
    desc.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace|rhi::AccelerationStructureBuildFlags::AllowCompaction;
    rhi::AccelerationStructureQueryDesc result{rhi::QueryType::AccelerationStructureCompactedSize,scene.compact_sizes.get(),i};
    commands->buildAccelerationStructure(desc,scene.blas[i],nullptr,pending.scratch,1,&result);commands->globalBarrier();
  }
  pending.phase=Build::Phase::Build;
}
void RayGeometry::State::stage_compact(Build& pending,rhi::ICommandEncoder* commands) {
  auto& scene=*pending.scene;std::vector<std::uint64_t> sizes(pending.batch_count);
  const auto live=live_bytes();
  auto limit=std::min(config.maximum_build_bytes,
      live<config.maximum_resident_bytes?config.maximum_resident_bytes-live:0);
  if(scene.allocation) {
    limit=std::min(limit,scene.allocation->bytes()+budget.scratch_bytes);
    scene.allocation->phase(SceneMemoryPhase::Compacting);
  }
  checked(scene.compact_sizes->getResult(pending.first_batch,pending.batch_count,sizes.data()),"ray staged compact sizes failed");
  for(std::uint32_t offset=0;offset<pending.batch_count;++offset) {
    const auto i=pending.first_batch+offset;const auto original=scene.blas[i]->getDesc().size;
    check(sizes[offset]>0,"empty ray staged compact size");
    const auto bytes=std::min(sizes[offset],original);if(bytes==original)continue;
    // If the final uncompressed remainder fits, compaction is optional. Never
    // allocate its temporary copy when that would exceed the same peak budget.
    if(pending.build_bytes+pending.tlas_reserve+bytes>limit)continue;
    auto compacted=acceleration(device,rhi::AccelerationStructureKind::BottomLevel,bytes);
    commands->copyAccelerationStructure(compacted,scene.blas[i],rhi::AccelerationStructureCopyMode::Compact);
    pending.compact_sources.push_back(scene.blas[i]);pending.compact_source_bytes+=original;
    scene.blas[i]=std::move(compacted);scene.bytes=scene.bytes-original+bytes;pending.build_bytes+=bytes;
  }
  commands->globalBarrier();pending.phase=Build::Phase::Compact;
}
bool RayGeometry::continuation_ready() const {return state_->pending && state_->pending->ready;}
bool RayGeometry::record_continue(rhi::ICommandEncoder* commands) {
  auto& s=*state_;try {
    check(commands&&s.pending&&s.pending->ready&&!s.pending->fence,"ray staged continuation is not ready");
    auto& pending=*s.pending;pending.ready=false;pending.recorded=false;s.budget.limited=false;
    if(pending.phase==State::Build::Phase::Build)s.stage_compact(pending,commands);
    else if(pending.next_batch<pending.scene->batches.size())s.stage_build(pending,commands);
    else {
      s.tlas(pending,commands);
      commands->setBufferState(pending.scene->vertices,rhi::ResourceState::ShaderResource);
      commands->setBufferState(pending.scene->indices,rhi::ResourceState::ShaderResource);
      pending.phase=State::Build::Phase::Complete;
    }
    s.budget.build_bytes=pending.build_bytes;s.budget.resident_bytes=s.live_bytes()+pending.build_bytes;
    pending.recorded=true;s.error.clear();return true;
  } catch(const std::exception& error) {s.error=error.what();return false;}
}
}
