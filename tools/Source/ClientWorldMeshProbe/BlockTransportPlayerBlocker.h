#pragma once
#include "BlockTransportSetup.h"
#include <slang-rhi/acceleration-structure-utils.h>

namespace mesh_probe {
struct BlockTransportPlayerBlocker {
  Slang::ComPtr<rhi::IBuffer> vertices,indices,instances,blas_scratch,tlas_scratch;
  Slang::ComPtr<rhi::IAccelerationStructure> blas,scene;
  explicit BlockTransportPlayerBlocker(WorldRenderer& r,float x=0,float y=0,float z=0) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT player blocker frame reuse");
    const float positions[8][4]={{10.75f,2.75f,10.75f,0},{11.25f,2.75f,10.75f,0},
        {10.75f,3.25f,10.75f,0},{11.25f,3.25f,10.75f,0},{10.75f,2.75f,11.25f,0},
        {11.25f,2.75f,11.25f,0},{10.75f,3.25f,11.25f,0},{11.25f,3.25f,11.25f,0}};
    const unsigned triangles[36]={0,2,3,0,3,1,4,5,7,4,7,6,0,4,6,0,6,2,
        1,3,7,1,7,5,0,1,5,0,5,4,2,6,7,2,7,3};
    const auto usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::AccelerationStructureBuildInput;
    vertices=buffer(r,positions,sizeof(positions),16,usage);indices=buffer(r,triangles,sizeof(triangles),4,usage);
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT player blocker encoder");
    const auto build=[&](rhi::AccelerationStructureBuildInput& input,rhi::AccelerationStructureKind kind,
        Slang::ComPtr<rhi::IAccelerationStructure>& target,Slang::ComPtr<rhi::IBuffer>& scratch) {
      rhi::AccelerationStructureBuildDesc description{};description.inputs=&input;description.inputCount=1;
      description.flags=rhi::AccelerationStructureBuildFlags::PreferFastBuild;
      rhi::AccelerationStructureSizes sizes{};
      checked(r.device->getAccelerationStructureSizes(description,&sizes),"BT player blocker AS size");
      require(sizes.accelerationStructureSize>0 && sizes.scratchSize>0,"BT player blocker AS allocation is empty");
      rhi::AccelerationStructureDesc allocation{};allocation.kind=kind;allocation.size=sizes.accelerationStructureSize;
      allocation.label="bt_probe_player_blocker";
      checked(r.device->createAccelerationStructure(allocation,target.writeRef()),"BT player blocker AS allocation");
      scratch=buffer(r,nullptr,std::size_t(sizes.scratchSize),4,rhi::BufferUsage::UnorderedAccess);
      commands->setBufferState(scratch,rhi::ResourceState::UnorderedAccess);
      commands->globalBarrier();commands->buildAccelerationStructure(description,target,nullptr,scratch,0,nullptr);
      commands->globalBarrier();
    };
    rhi::AccelerationStructureBuildInput geometry{};geometry.type=rhi::AccelerationStructureBuildInputType::Triangles;
    geometry.triangles.vertexBuffers[0]=vertices;geometry.triangles.vertexBufferCount=1;
    geometry.triangles.vertexFormat=rhi::Format::RGB32Float;geometry.triangles.vertexStride=16;geometry.triangles.vertexCount=8;
    geometry.triangles.indexBuffer=indices;geometry.triangles.indexFormat=rhi::IndexFormat::Uint32;geometry.triangles.indexCount=36;
    geometry.triangles.flags=rhi::AccelerationStructureGeometryFlags::Opaque;
    commands->setBufferState(vertices,rhi::ResourceState::AccelerationStructureBuildInput);
    commands->setBufferState(indices,rhi::ResourceState::AccelerationStructureBuildInput);
    build(geometry,rhi::AccelerationStructureKind::BottomLevel,blas,blas_scratch);
    rhi::AccelerationStructureInstanceDescGeneric instance{};
    instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
    instance.transform[0][3]=x;instance.transform[1][3]=y;instance.transform[2][3]=z;
    instance.instanceMask=255;instance.accelerationStructure=blas->getHandle();
    const auto type=rhi::getAccelerationStructureInstanceDescType(r.device);
    const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);std::vector<std::uint8_t> native(stride);
    rhi::convertAccelerationStructureInstanceDescs(1,type,native.data(),stride,&instance,sizeof(instance));
    instances=buffer(r,native.data(),native.size(),unsigned(stride),usage);
    commands->setBufferState(instances,rhi::ResourceState::AccelerationStructureBuildInput);
    rhi::AccelerationStructureBuildInput top{};top.type=rhi::AccelerationStructureBuildInputType::Instances;
    top.instances.instanceBuffer=instances;top.instances.instanceStride=unsigned(stride);top.instances.instanceCount=1;
    build(top,rhi::AccelerationStructureKind::TopLevel,scene,tlas_scratch);
    auto command=commands->finish();require(bool(command),"BT player blocker command finish");
    require(r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BT player blocker bounded GPU build completion");
    block_transport_complete(r,start);
  }
};
}
