#include "WorldRayTracingState.h"
namespace octaryn::client::rendering {
bool WorldRayTracing::State::poll(WorldRenderer& r) {
  for(auto& job:jobs) {
    if(!job.pending)continue;
    const auto found=r.columns.find(job.coordinate);
    if(found==r.columns.end() || !job.pending->matches(found->second)) {
      if(!job.cancelled)bytes_dirty=true;
      job.cancelled=true;
    }
    rhi::IFence* completed=fence;
    const auto result=r.device->waitForFences(1,&completed,&job.signal,true,0);
    if(result==SLANG_E_TIME_OUT)continue;
    if(!world_rhi_ok(result))return false;
    if(!job.timing.resolve(stats.blas_gpu_ms))return false;
    if(!job.cancelled) {
      columns[job.coordinate]=job.pending;++generation;
      changed.erase(job.coordinate);
      r.scene_changes.notify_column(job.coordinate.first,job.coordinate.second,
        found->second.min_y,found->second.height,SceneChangeKind::AccelerationReady);
    }
    else ++stats.discarded_builds;
    // The exact fence permits scratch reuse while snapshots retain immutable BLAS.
    job.submission.setNull();job.pending.reset();job.refit_source.reset();bytes_dirty=true;
  }
    return true;
  }
bool WorldRayTracing::State::start(WorldRenderer& r,Coord coord,const WorldColumnGpu& source) {
    const auto slot=std::find_if(jobs.begin(),jobs.end(),[](const BuildJob& job){return !job.pending;});
    if(slot==jobs.end())return false;
    auto& job=*slot;auto& refit_source=job.refit_source;
    auto& bounds=job.bounds;auto& scratch=job.scratch;auto& submission=job.submission;
    auto column=std::make_shared<Column>();column->faces=source.faces;column->fluids=source.fluids;
    column->record.face_count=source.face_count;
    column->record.fluid_base=source.pass_counts[0]+source.pass_counts[1]+source.pass_counts[2];
    column->record.reserved[0]=static_cast<std::uint32_t>(coord.first);
    column->record.reserved[1]=static_cast<std::uint32_t>(coord.second);
    const auto old=changed.find(coord);
    refit_source=old!=changed.end() && old->second->record.face_count==source.face_count && old->second->refits<8?
      old->second:nullptr;
    if(old!=changed.end())changed.erase(old);
    if(!descriptor(column->faces,column->record.faces) || !descriptor(column->fluids,column->record.fluids))return false;
    const auto usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::AccelerationStructureBuildInput;
    if(!buffer(r,std::uint64_t(source.face_count)*24,24,usage,rhi::ResourceState::AccelerationStructureBuildInput,bounds))return false;
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::ProceduralPrimitives;
    input.proceduralPrimitives.aabbBuffers[0]=bounds;input.proceduralPrimitives.aabbBufferCount=1;
    input.proceduralPrimitives.aabbStride=24;input.proceduralPrimitives.primitiveCount=source.face_count;
    input.proceduralPrimitives.flags=rhi::AccelerationStructureGeometryFlags::None;
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace|rhi::AccelerationStructureBuildFlags::AllowUpdate;
    rhi::AccelerationStructureSizes sizes{};
    if(!world_rhi_ok(r.device->getAccelerationStructureSizes(build,&sizes)) || !sizes.accelerationStructureSize)return false;
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::BottomLevel;
    desc.size=sizes.accelerationStructureSize;desc.label="world_ray_column";
    if(!world_rhi_ok(r.device->createAccelerationStructure(desc,column->blas.writeRef())) ||
       !buffer(r,std::max(sizes.scratchSize,sizes.updateScratchSize),4,rhi::BufferUsage::UnorderedAccess,rhi::ResourceState::UnorderedAccess,scratch))return false;
    auto commands=r.queue->createCommandEncoder();if(!commands)return false;
    if(!job.timing.begin(r.device,commands,r.capabilities.timestamps))return false;
    auto* compute=commands->beginComputePass();if(!compute)return false;
    auto* root=compute->bindPipeline(bounds_pipeline);
    bool success=root && bind_buffer(root,"rayFaces",column->faces) && bind_buffer(root,"rayFluids",column->fluids) &&
      bind_buffer(root,"rayBounds",bounds) && bind_buffer(root,"blockMaterials",world_atlas_materials(r.atlas)) &&
      world_rhi_ok(rhi::ShaderCursor(root)["rayFaceCount"].setData(&column->record.face_count,4)) &&
      world_rhi_ok(rhi::ShaderCursor(root)["rayFluidBase"].setData(&column->record.fluid_base,4));
    if(success)compute->dispatchCompute((source.face_count+63)/64,1,1);
    compute->end();if(!success)return false;
    if(refit_source) {
      build.mode=rhi::AccelerationStructureBuildMode::Update;column->refits=refit_source->refits+1;
    }
    commands->buildAccelerationStructure(build,column->blas,refit_source?refit_source->blas.get():nullptr,scratch,0,nullptr);
    commands->globalBarrier();
    job.timing.end(commands);
    submission=commands->finish();if(!submission)return false;
    rhi::ICommandBuffer* command=submission;rhi::IFence* completed=fence;const auto value=signal+1;
    rhi::SubmitDesc submit{};submit.commandBuffers=&command;submit.commandBufferCount=1;
    submit.signalFences=&completed;submit.signalFenceValues=&value;submit.signalFenceCount=1;
    if(!world_rhi_ok(r.queue->submit(submit)))return false;
    signal=value;job.signal=value;job.pending=std::move(column);job.coordinate=coord;job.cancelled=false;
    if(refit_source)++stats.blas_refits;else ++stats.blas_builds;
    bytes_dirty=true;
    return true;
  }
}
