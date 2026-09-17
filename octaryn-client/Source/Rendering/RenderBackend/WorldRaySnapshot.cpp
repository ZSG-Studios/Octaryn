#include "WorldRayTracingState.h"
#include <cstdio>
#include <slang-rhi/acceleration-structure-utils.h>
namespace octaryn::client::rendering {
bool WorldRayTracing::State::empty_blas(WorldRenderer& r,rhi::ICommandEncoder* commands,Frame& frame) {
    if(dummy)return true;
    const float box[6]={0,0,0,1,1,1};
    if(!buffer(r,sizeof(box),sizeof(box),rhi::BufferUsage::AccelerationStructureBuildInput|rhi::BufferUsage::CopyDestination,
      rhi::ResourceState::AccelerationStructureBuildInput,frame.dummy_bounds) ||
      !world_rhi_ok(commands->uploadBufferData(frame.dummy_bounds,0,sizeof(box),box)))return false;
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::ProceduralPrimitives;
    input.proceduralPrimitives.aabbBuffers[0]=frame.dummy_bounds;input.proceduralPrimitives.aabbBufferCount=1;
    input.proceduralPrimitives.aabbStride=sizeof(box);input.proceduralPrimitives.primitiveCount=1;
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    rhi::AccelerationStructureSizes sizes{};
    if(!world_rhi_ok(r.device->getAccelerationStructureSizes(build,&sizes)) || !sizes.accelerationStructureSize)return false;
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::BottomLevel;
    desc.size=sizes.accelerationStructureSize;desc.label="world_ray_masked_empty";
    if(!world_rhi_ok(r.device->createAccelerationStructure(desc,dummy.writeRef())) ||
      !buffer(r,sizes.scratchSize,4,rhi::BufferUsage::UnorderedAccess,rhi::ResourceState::UnorderedAccess,frame.dummy_scratch))return false;
    commands->buildAccelerationStructure(build,dummy,nullptr,frame.dummy_scratch,0,nullptr);
    return true;
  }

bool WorldRayTracing::State::snapshot(WorldRenderer& r,rhi::ICommandEncoder* commands,Frame& frame) {
    if(current && current->generation==generation) {frame.snapshot=current;return true;}
    if(!frame.timing.begin(r.device,commands,r.capabilities.timestamps)) {std::fprintf(stderr,"snapshot_failed step=timing\n");return false;}
    auto next=std::make_shared<Snapshot>();next->generation=generation;
    if(columns.size()>0xFFFFFFu)return false;
    std::vector<Record> records;std::vector<rhi::AccelerationStructureInstanceDescGeneric> generic;
    records.reserve(std::max<std::size_t>(1,columns.size()));generic.reserve(columns.size());
    for(const auto& [coord,column]:columns) {
      rhi::AccelerationStructureInstanceDescGeneric instance{};
      instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
      instance.instanceID=static_cast<std::uint32_t>(records.size());instance.instanceMask=0xFF;
      instance.accelerationStructure=column->blas->getHandle();
      records.push_back(column->record);generic.push_back(instance);next->columns.push_back(column);
    }
    if(records.empty()) {
      if(!empty_blas(r,commands,frame))return false;
      records.emplace_back();
      rhi::AccelerationStructureInstanceDescGeneric instance{};
      instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
      // The pinned RHI requires a nonempty TLAS; no ray can visit this instance.
      instance.instanceMask=0;instance.accelerationStructure=dummy->getHandle();generic.push_back(instance);
    }
    const auto type=rhi::getAccelerationStructureInstanceDescType(r.device);
    const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
    std::vector<std::uint8_t> native(std::max<std::size_t>(1,generic.size())*stride);
    if(!generic.empty())rhi::convertAccelerationStructureInstanceDescs(generic.size(),type,native.data(),stride,
      generic.data(),sizeof(rhi::AccelerationStructureInstanceDescGeneric));
    if(!buffer(r,records.size()*sizeof(Record),sizeof(Record),rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination,
         rhi::ResourceState::ShaderResource,next->records) ||
       !buffer(r,native.size(),static_cast<unsigned>(stride),rhi::BufferUsage::AccelerationStructureBuildInput|rhi::BufferUsage::CopyDestination,
         rhi::ResourceState::AccelerationStructureBuildInput,frame.instances)) {std::fprintf(stderr,"snapshot_failed step=records_buffers\n");return false;}
    // Initial-data creation can perform a synchronous upload inside the backend.
    if(!world_rhi_ok(commands->uploadBufferData(next->records,0,records.size()*sizeof(Record),records.data())) ||
       !world_rhi_ok(commands->uploadBufferData(frame.instances,0,native.size(),native.data()))) {std::fprintf(stderr,"snapshot_failed step=uploads\n");return false;}
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
    input.instances.instanceBuffer=frame.instances;input.instances.instanceStride=static_cast<unsigned>(stride);
    input.instances.instanceCount=static_cast<std::uint32_t>(generic.size());
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
    rhi::AccelerationStructureSizes sizes{};
    if(!world_rhi_ok(r.device->getAccelerationStructureSizes(build,&sizes)) || !sizes.accelerationStructureSize) {std::fprintf(stderr,"snapshot_failed step=tlas_sizes\n");return false;}
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::TopLevel;
    desc.size=sizes.accelerationStructureSize;desc.label="world_ray_scene";
    if(!world_rhi_ok(r.device->createAccelerationStructure(desc,next->tlas.writeRef())) ||
       !buffer(r,std::max(sizes.scratchSize,sizes.updateScratchSize),4,rhi::BufferUsage::UnorderedAccess,rhi::ResourceState::UnorderedAccess,frame.scratch)) {std::fprintf(stderr,"snapshot_failed step=tlas_create\n");return false;}
    // TLAS references BLAS through device addresses, invisible to automatic tracking.
    commands->globalBarrier();
    // Always full-build into this newly created TLAS. Update mode requires the
    // destination to already contain a compatible build; dest here is empty.
    commands->buildAccelerationStructure(build,next->tlas,nullptr,frame.scratch,0,nullptr);
    commands->globalBarrier();
    current=next;frame.snapshot=std::move(next);
    frame.timing.end(commands);
    ++stats.tlas_builds;
    bytes_dirty=true;
    return true;
  }
}
