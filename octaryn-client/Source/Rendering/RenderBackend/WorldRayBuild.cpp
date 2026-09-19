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
    std::uint64_t completed{};
    RayPrepareDiagnostics diagnostic{"blas_poll"};
    diagnostic.frame=r.frames;diagnostic.generation=generation;
    diagnostic.x=job.coordinate.first;diagnostic.z=job.coordinate.second;
    diagnostic.faces=job.pending->record.face_count;diagnostic.signal=job.signal;
    const auto result=fence->getCurrentValue(&completed);diagnostic.observed=completed;
    if(!diagnostic.check("fence_value",result) ||
        !diagnostic.require("fence_device_alive",completed!=UINT64_MAX))return false;
    if(completed<job.signal)continue;
    if(!job.timing.resolve(stats.blas_gpu_ms,&diagnostic))return false;
    if(!job.cancelled) {
      columns[job.coordinate]=job.pending;++generation;
      changed.erase(job.coordinate);
      // Equal counts do not prove equal geometry: moving a wall can preserve
      // every count. Skip occlusion refresh only when neither mesh has blockers.
      const auto known=built_pass_counts.find(job.coordinate);
      const auto& counts=found->second.pass_counts;
      const bool minor=known!=built_pass_counts.end() &&
          known->second[0]==0 && counts[0]==0 && known->second[4]==0 && counts[4]==0;
      built_pass_counts[job.coordinate]=counts;
      r.scene_changes.notify_column(job.coordinate.first,job.coordinate.second,
        found->second.min_y,found->second.height,SceneChangeKind::AccelerationReady,minor);
    }
    else ++stats.discarded_builds;
    // The exact fence permits scratch reuse while snapshots retain immutable BLAS.
    job.submission.setNull();job.pending.reset();job.refit_source.reset();bytes_dirty=true;
  }
    return true;
  }
bool WorldRayTracing::State::start(WorldRenderer& r,Coord coord,const WorldColumnGpu& source) {
    RayPrepareDiagnostics diagnostic{"blas_build"};
    diagnostic.frame=r.frames;diagnostic.generation=generation;
    diagnostic.x=coord.first;diagnostic.z=coord.second;diagnostic.faces=source.face_count;
    const auto slot=std::find_if(jobs.begin(),jobs.end(),[](const BuildJob& job){return !job.pending;});
    if(!diagnostic.require("free_job",slot!=jobs.end()))return false;
    auto& job=*slot;
    auto& bounds=job.bounds;auto& scratch=job.scratch;auto& submission=job.submission;
    auto column=std::make_shared<Column>();column->faces=source.faces;column->fluids=source.fluids;
    column->record.face_count=source.face_count;
    column->record.fluid_base=source.pass_counts[0]+source.pass_counts[1]+source.pass_counts[2];
    column->record.reserved[0]=static_cast<std::uint32_t>(coord.first);
    column->record.reserved[1]=static_cast<std::uint32_t>(coord.second);
    const auto old=changed.find(coord);
    if(old!=changed.end())changed.erase(old);
    if(!descriptor(column->faces,column->record.faces,diagnostic,"faces_descriptor") ||
       !descriptor(column->fluids,column->record.fluids,diagnostic,"fluids_descriptor"))return false;
    const auto usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::AccelerationStructureBuildInput;
    if(!buffer(r,std::uint64_t(source.face_count)*24,24,usage,rhi::ResourceState::AccelerationStructureBuildInput,bounds,
        &diagnostic,"bounds_buffer"))return false;
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::ProceduralPrimitives;
    input.proceduralPrimitives.aabbBuffers[0]=bounds;input.proceduralPrimitives.aabbBufferCount=1;
    input.proceduralPrimitives.aabbStride=24;input.proceduralPrimitives.primitiveCount=source.face_count;
    input.proceduralPrimitives.flags=rhi::AccelerationStructureGeometryFlags::None;
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
    rhi::AccelerationStructureSizes sizes{};
    if(!diagnostic.check("blas_sizes",r.device->getAccelerationStructureSizes(build,&sizes)) ||
       !diagnostic.require("blas_size_nonzero",sizes.accelerationStructureSize!=0))return false;
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::BottomLevel;
    desc.size=sizes.accelerationStructureSize;desc.label="world_ray_column";
    diagnostic.bytes=desc.size;
    if(!diagnostic.check("blas_create",r.device->createAccelerationStructure(desc,column->blas.writeRef())) ||
       !buffer(r,sizes.scratchSize,4,rhi::BufferUsage::UnorderedAccess,rhi::ResourceState::UnorderedAccess,scratch,
         &diagnostic,"blas_scratch"))return false;
    auto commands=r.queue->createCommandEncoder();if(!diagnostic.require("encoder",bool(commands)))return false;
    if(!job.timing.begin(r.device,commands,r.capabilities.timestamps,&diagnostic))return false;
    auto* compute=commands->beginComputePass();if(!diagnostic.require("compute_pass",compute!=nullptr))return false;
    auto* root=compute->bindPipeline(bounds_pipeline);
    bool success=diagnostic.require("bounds_pipeline",root!=nullptr) &&
      bind_buffer(root,"rayFaces",column->faces,&diagnostic) && bind_buffer(root,"rayFluids",column->fluids,&diagnostic) &&
      bind_buffer(root,"rayBounds",bounds,&diagnostic) && bind_buffer(root,"blockMaterials",world_atlas_materials(r.atlas),&diagnostic) &&
      diagnostic.check("face_count",rhi::ShaderCursor(root)["rayFaceCount"].setData(&column->record.face_count,4)) &&
      diagnostic.check("fluid_base",rhi::ShaderCursor(root)["rayFluidBase"].setData(&column->record.fluid_base,4));
    if(success)compute->dispatchCompute((source.face_count+63)/64,1,1);
    compute->end();if(!diagnostic.require("bounds_bindings",success))return false;
    commands->setBufferState(bounds,rhi::ResourceState::AccelerationStructureBuildInput);
    commands->globalBarrier();
    commands->buildAccelerationStructure(build,column->blas,nullptr,scratch,0,nullptr);
    commands->globalBarrier();
    job.timing.end(commands);
    submission=commands->finish();if(!diagnostic.require("finish",bool(submission)))return false;
    rhi::ICommandBuffer* command=submission;rhi::IFence* completed=fence;const auto value=signal+1;
    rhi::SubmitDesc submit{};submit.commandBuffers=&command;submit.commandBufferCount=1;
    submit.signalFences=&completed;submit.signalFenceValues=&value;submit.signalFenceCount=1;
    diagnostic.signal=value;
    if(!diagnostic.check("submit",r.queue->submit(submit)))return false;
    signal=value;job.signal=value;job.pending=std::move(column);job.coordinate=coord;job.cancelled=false;
    ++stats.blas_builds;
    bytes_dirty=true;
    return true;
  }
}
