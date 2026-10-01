#include "WorldRayTracingState.h"
namespace octaryn::client::rendering {
bool WorldRayTracing::State::poll(WorldRenderer& r) {
  // Column BLAS builds are fed by world geometry streaming; with no feeder,
  // no job is ever pending and this only advances the allocation worker.
  return progress_allocations(r,UINT64_MAX);
  }
bool WorldRayTracing::State::submit(WorldRenderer& r,BuildJob& job) {
    RayPrepareDiagnostics diagnostic{"blas_build"};diagnostic.frame=r.frames;diagnostic.generation=generation;
    diagnostic.x=job.coordinate.first;diagnostic.z=job.coordinate.second;diagnostic.faces=job.pending->record.face_count;
    auto& column=job.pending;auto& bounds=job.bounds;auto& scratch=job.scratch;auto& submission=job.submission;
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::ProceduralPrimitives;
    input.proceduralPrimitives.aabbBuffers[0]=bounds;input.proceduralPrimitives.aabbBufferCount=1;
    input.proceduralPrimitives.aabbStride=24;input.proceduralPrimitives.primitiveCount=column->record.face_count;
    input.proceduralPrimitives.flags=rhi::AccelerationStructureGeometryFlags::None;
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
    auto commands=r.queue->createCommandEncoder();if(!diagnostic.require("encoder",bool(commands)))return false;
    if(!job.timing.begin(r.device,commands,r.capabilities.timestamps,&diagnostic))return false;
    auto* compute=commands->beginComputePass();if(!diagnostic.require("compute_pass",compute!=nullptr))return false;
    auto* root=compute->bindPipeline(bounds_pipeline);
    bool success=diagnostic.require("bounds_pipeline",root!=nullptr) &&
      bind_buffer(root,"rayFaces",column->faces,&diagnostic) &&
      bind_buffer(root,"rayBounds",bounds,&diagnostic) && bind_buffer(root,"blockMaterials",world_atlas_materials(r.atlas),&diagnostic) &&
      diagnostic.check("face_count",rhi::ShaderCursor(root)["rayFaceCount"].setData(&column->record.face_count,4));
    if(success)compute->dispatchCompute((column->record.face_count+63)/64,1,1);
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
    signal=value;job.signal=value;
    ++stats.blas_builds;
    bytes_dirty=true;
    return true;
  }
}
