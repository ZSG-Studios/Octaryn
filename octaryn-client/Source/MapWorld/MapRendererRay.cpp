#include "MapRendererInternal.h"
#include "FrameWatchdog.h"
#include "MapRayResources.h"
#include "MapRaySubmitScope.h"
#include "../Threading/BackgroundThread.h"
#include <slang-rhi/acceleration-structure-utils.h>
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace octaryn::client::rendering {
// 'MAP' TLAS instance user id; WorldRayQuery branches triangle hits on it.
static constexpr std::uint32_t map_ray_instance_id=0x800000u;
bool map_ray_ready(const MapRenderer& map) {return map.ray_ready && map.tlas && map.blas;}
rhi::IAccelerationStructure* map_ray_blas(const MapRenderer& map) {return map.blas.get();}
bool map_ray_geometry(const MapRenderer& map,MapRayGeometry& result) {
  const auto descriptor=[](rhi::IBuffer* buffer,std::uint64_t& value) {
    rhi::DescriptorHandle handle{};
    if(!buffer || SLANG_FAILED(buffer->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,
        rhi::Format::Undefined,rhi::kEntireBuffer,&handle)) || handle.type!=rhi::DescriptorHandleType::Buffer)return false;
    value=handle.value;return true;
  };
  return descriptor(map.vertices,result.vertices) && descriptor(map.indices,result.indices) &&
      descriptor(map.ray_primitives,result.materials);
}
bool bind_map_ray_buffers(MapRenderer& map,rhi::IShaderObject* root) {
  if(!map.ray_ready)return true;
  rhi::ShaderCursor cursor(root);
  const auto bind=[&](const char* name,rhi::IBuffer* buffer) {
    auto field=cursor[name];
    return !field.isValid() || SLANG_SUCCEEDED(field.setBinding(rhi::Binding(buffer)));
  };
  return bind("mapVertices",map.vertices.get()) && bind("mapIndices",map.indices.get()) &&
      bind("mapRayPrimitives",map.ray_primitives.get());
}
bool prepare_map_ray_scene(MapRenderer& map,rhi::ICommandEncoder* commands) {
  // Nothing to do once built or when the device lacks acceleration structures.
  if(map.ray_ready || !map.ray_supported)return true;
  if(map.ray_pending_fence)return false;
  if(!commands)return false;
  const auto fail=[](const char* stage) {
    std::fprintf(stderr,"map_ray_prepare_failed stage=%s\n",stage);
    return false;
  };
  if(!map.ray_resources_ready && !allocate_map_ray_resources(map,nullptr))return fail("resources");
  std::vector<rhi::AccelerationStructureBuildInput> inputs;
  if(!map_ray_inputs(map,inputs))return fail("primitive_range");
  rhi::AccelerationStructureBuildDesc build{};
  build.inputs=inputs.data();build.inputCount=unsigned(inputs.size());
  build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace|rhi::AccelerationStructureBuildFlags::AllowCompaction;
  rhi::AccelerationStructureInstanceDescGeneric instance{};
  instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
  instance.instanceID=map_ray_instance_id;instance.instanceMask=255;
  instance.accelerationStructure=map.blas->getHandle();
  const auto type=rhi::getAccelerationStructureInstanceDescType(map.device.get());
  const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
  std::vector<std::uint8_t> native(stride);
  rhi::convertAccelerationStructureInstanceDescs(1,type,native.data(),stride,&instance,sizeof(instance));
  if(SLANG_FAILED(commands->uploadBufferData(map.instances,0,stride,native.data())))return fail("instances_upload");
  rhi::AccelerationStructureBuildInput scene_input{};
  scene_input.type=rhi::AccelerationStructureBuildInputType::Instances;
  scene_input.instances.instanceBuffer=map.instances;
  scene_input.instances.instanceStride=unsigned(stride);scene_input.instances.instanceCount=1;
  rhi::AccelerationStructureBuildDesc scene{};scene.inputs=&scene_input;scene.inputCount=1;
  scene.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
  // Static geometry: the BLAS inputs stay in place for the whole session.
  commands->setBufferState(map.vertices,rhi::ResourceState::AccelerationStructureBuildInput);
  commands->setBufferState(map.indices,rhi::ResourceState::AccelerationStructureBuildInput);
  commands->globalBarrier();
  rhi::AccelerationStructureQueryDesc query{rhi::QueryType::AccelerationStructureCompactedSize,map.compact_size.get(),0};
  commands->buildAccelerationStructure(build,map.blas,nullptr,map.blas_scratch,1,&query);
  commands->globalBarrier();
  commands->buildAccelerationStructure(scene,map.tlas,nullptr,map.tlas_scratch,0,nullptr);
  commands->globalBarrier();
  commands->setBufferState(map.vertices,rhi::ResourceState::ShaderResource);
  commands->setBufferState(map.indices,rhi::ResourceState::ShaderResource);
  return true;
}
bool submit_map_ray_compaction(MapRenderer& map,rhi::ICommandQueue* queue,bool asynchronous_allocation,
    const MapRaySubmitScope* profile) {
  Slang::ComPtr<rhi::IAccelerationStructure> compact;
  if(map.compact_allocation.valid()) {
    if(map.compact_allocation.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return true;
    try {compact=map.compact_allocation.get();}catch(...) {return false;}
    if(!compact)return false;
  } else {
    if(!map.compact_size)return true;
    std::uint64_t size{};
    if(SLANG_FAILED(map.compact_size->getResult(0,1,&size)) || !size)return false;
    map.compact_size.setNull();map.blas_scratch.setNull();
    if(size>=map.blas->getDesc().size)return true;
    auto allocation=map.blas->getDesc();allocation.size=size;allocation.label="map_ray_blas_compact";
    if(asynchronous_allocation) {
      map.compact_allocation=std::async(std::launch::async,[device=map.device,allocation] {
      threading::set_background_thread_priority("map_compaction_allocation");
      const auto start=std::chrono::steady_clock::now();
      Slang::ComPtr<rhi::IAccelerationStructure> result;
      if(SLANG_FAILED(device->createAccelerationStructure(allocation,result.writeRef())))return result;
      std::printf("map_compact_resource cpu_ms=%.3f bytes=%llu\n",
          std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),allocation.size);
      return result;
      });
      return true;
    }
    if(SLANG_FAILED(map.device->createAccelerationStructure(allocation,compact.writeRef())))return false;
  }
  auto commands=queue->createCommandEncoder();if(!commands)return false;
  if(profile && !profile->begin(commands,MapRaySubmitKind::Compaction))return false;
  commands->copyAccelerationStructure(compact,map.blas,rhi::AccelerationStructureCopyMode::Compact);
  commands->globalBarrier();
  rhi::AccelerationStructureInstanceDescGeneric instance{};
  instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
  instance.instanceID=map_ray_instance_id;instance.instanceMask=255;
  instance.accelerationStructure=compact->getHandle();
  const auto type=rhi::getAccelerationStructureInstanceDescType(map.device.get());
  const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
  std::vector<std::uint8_t> native(stride);
  rhi::convertAccelerationStructureInstanceDescs(1,type,native.data(),stride,&instance,sizeof(instance));
  if(SLANG_FAILED(commands->uploadBufferData(map.instances,0,stride,native.data())))return false;
  commands->setBufferState(map.instances,rhi::ResourceState::AccelerationStructureBuildInput);
  rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
  input.instances.instanceBuffer=map.instances;input.instances.instanceStride=unsigned(stride);input.instances.instanceCount=1;
  rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
  build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
  commands->buildAccelerationStructure(build,map.tlas,nullptr,map.tlas_scratch,0,nullptr);
  commands->globalBarrier();
  if(profile && !profile->end(commands))return false;
  auto submission=commands->finish();if(!submission)return false;
  Slang::ComPtr<rhi::IFence> fence;
  if(SLANG_FAILED(map.device->createFence({},fence.writeRef())))return false;
  rhi::ICommandBuffer* buffer=submission.get();rhi::IFence* signal=fence.get();const std::uint64_t value=1;
  rhi::SubmitDesc submit{};submit.commandBuffers=&buffer;submit.commandBufferCount=1;
  submit.signalFences=&signal;submit.signalFenceValues=&value;submit.signalFenceCount=1;
  if(SLANG_FAILED(queue->submit(submit)))frame_gpu_shutdown_failed("map_compact_submit");
  // The source BLAS and TLAS scratch remain owned until the copy/build fence retires.
  map.uncompacted_blas=map.blas;map.blas=compact;
  map.ray_pending_fence=fence;map.ray_pending_commands=submission;
  map.ray_submitted_at=std::chrono::steady_clock::now();map.ray_ready=false;
  std::printf("map_ray_compaction original_bytes=%llu compact_bytes=%llu\n",
      static_cast<unsigned long long>(map.uncompacted_blas->getDesc().size),static_cast<unsigned long long>(compact->getDesc().size));
  std::fflush(stdout);return true;
}

}
