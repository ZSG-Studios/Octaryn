#include "WorldRayTracingState.h"
#include <cstdio>
#include <slang-rhi/acceleration-structure-utils.h>
namespace octaryn::client::rendering {
bool WorldRayTracing::State::empty_blas(WorldRenderer& r,rhi::ICommandEncoder* commands,Frame& frame) {
    if(dummy)return true;
    RayPrepareDiagnostics diagnostic{"empty_blas"};diagnostic.frame=r.frames;diagnostic.generation=generation;
    const float box[6]={0,0,0,1,1,1};
    if(!buffer(r,sizeof(box),sizeof(box),rhi::BufferUsage::AccelerationStructureBuildInput|rhi::BufferUsage::CopyDestination,
      rhi::ResourceState::AccelerationStructureBuildInput,frame.dummy_bounds,&diagnostic,"bounds_buffer") ||
      !diagnostic.check("bounds_upload",commands->uploadBufferData(frame.dummy_bounds,0,sizeof(box),box)))return false;
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::ProceduralPrimitives;
    input.proceduralPrimitives.aabbBuffers[0]=frame.dummy_bounds;input.proceduralPrimitives.aabbBufferCount=1;
    input.proceduralPrimitives.aabbStride=sizeof(box);input.proceduralPrimitives.primitiveCount=1;
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    rhi::AccelerationStructureSizes sizes{};
    if(!diagnostic.check("blas_sizes",r.device->getAccelerationStructureSizes(build,&sizes)) ||
       !diagnostic.require("blas_size_nonzero",sizes.accelerationStructureSize!=0))return false;
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::BottomLevel;
    desc.size=sizes.accelerationStructureSize;desc.label="world_ray_masked_empty";
    diagnostic.bytes=desc.size;
    if(!diagnostic.check("blas_create",r.device->createAccelerationStructure(desc,dummy.writeRef())) ||
       !buffer(r,sizes.scratchSize,4,rhi::BufferUsage::UnorderedAccess,rhi::ResourceState::UnorderedAccess,frame.dummy_scratch,
         &diagnostic,"blas_scratch"))return false;
    commands->buildAccelerationStructure(build,dummy,nullptr,frame.dummy_scratch,0,nullptr);
    return true;
  }

bool WorldRayTracing::State::snapshot(WorldRenderer& r,rhi::ICommandEncoder* commands,Frame& frame,
    std::shared_ptr<Snapshot> reusable) {
    // Only the caller's completed, exclusively owned frame snapshot arrives here.
    // Cache it before the unchanged-scene return, which previously destroyed it.
    if(reusable) {
      reusable->columns.clear();reusable->map_blas.setNull();reusable->generation=0;
      const auto capacity=[](const Snapshot& scene) {
        return (scene.tlas?scene.tlas->getDesc().size:0)+(scene.records?scene.records->getDesc().size:0);
      };
      if(!spare || capacity(*reusable)>capacity(*spare))spare=std::move(reusable);
      bytes_dirty=true;
    }
    auto* map_blas=r.map && map_ray_ready(*r.map)?map_ray_blas(*r.map):nullptr;
    if(current && current->generation==generation && current->map_blas.get()==map_blas) {frame.snapshot=current;return true;}
    RayPrepareDiagnostics diagnostic{"snapshot"};diagnostic.frame=r.frames;diagnostic.generation=generation;
    if(!frame.timing.begin(r.device,commands,r.capabilities.timestamps,&diagnostic))return false;
    auto next=spare && spare.use_count()==1?std::move(spare):std::make_shared<Snapshot>();
    next->generation=generation;next->map_blas=map_blas;next->columns.clear();
    if(!diagnostic.require("instance_count",columns.size()<=0xFFFFFFu))return false;
    auto& records=snapshot_records;auto& generic=snapshot_instances;
    records.clear();generic.clear();
    for(const auto& [coord,column]:columns) {
      rhi::AccelerationStructureInstanceDescGeneric instance{};
      instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
      instance.instanceID=static_cast<std::uint32_t>(records.size());instance.instanceMask=0xFF;
      instance.accelerationStructure=column->blas->getHandle();
      records.push_back(column->record);generic.push_back(instance);next->columns.push_back(column);
    }
    // Map mode: one static 'MAP'-flagged triangle instance for the whole GLB.
    if(map_blas) {
      rhi::AccelerationStructureInstanceDescGeneric instance{};
      instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
      instance.instanceID=0x4D4150;instance.instanceMask=0xFF;
      instance.accelerationStructure=map_blas->getHandle();
      generic.push_back(instance);
    }
    if(records.empty()) {
      records.emplace_back();
      if(!map_blas) {
        if(!empty_blas(r,commands,frame))return false;
        rhi::AccelerationStructureInstanceDescGeneric instance{};
        instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
        // The pinned RHI requires a nonempty TLAS; no ray can visit this instance.
        instance.instanceMask=0;instance.accelerationStructure=dummy->getHandle();generic.push_back(instance);
      }
    }
    const auto type=rhi::getAccelerationStructureInstanceDescType(r.device);
    const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
    if(!diagnostic.require("instance_stride",stride>0))return false;
    auto& native=snapshot_native;native.resize(std::max<std::size_t>(1,generic.size())*stride);
    if(!generic.empty())rhi::convertAccelerationStructureInstanceDescs(generic.size(),type,native.data(),stride,
      generic.data(),sizeof(rhi::AccelerationStructureInstanceDescGeneric));
    const auto record_bytes=records.size()*sizeof(Record);
    const bool allocate_records=!next->records || next->records->getDesc().size<record_bytes;
    if(!buffer(r,record_bytes,sizeof(Record),rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination,
         rhi::ResourceState::ShaderResource,next->records,&diagnostic,"records_buffer"))return false;
    if(allocate_records)++stats.snapshot_record_allocations;
    if(!buffer(r,native.size(),static_cast<unsigned>(stride),rhi::BufferUsage::AccelerationStructureBuildInput|rhi::BufferUsage::CopyDestination,
          rhi::ResourceState::AccelerationStructureBuildInput,frame.instances,&diagnostic,"instances_buffer"))return false;
    // Initial-data creation can perform a synchronous upload inside the backend.
    diagnostic.bytes=records.size()*sizeof(Record);
    if(!diagnostic.check("records_upload",commands->uploadBufferData(next->records,0,diagnostic.bytes,records.data())))return false;
    diagnostic.bytes=native.size();
    if(!diagnostic.check("instances_upload",commands->uploadBufferData(frame.instances,0,native.size(),native.data())))return false;
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
    input.instances.instanceBuffer=frame.instances;input.instances.instanceStride=static_cast<unsigned>(stride);
    input.instances.instanceCount=static_cast<std::uint32_t>(generic.size());
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
    rhi::AccelerationStructureSizes sizes{};
    if(!diagnostic.check("tlas_sizes",r.device->getAccelerationStructureSizes(build,&sizes)) ||
       !diagnostic.require("tlas_size_nonzero",sizes.accelerationStructureSize!=0))return false;
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::TopLevel;
    desc.size=std::bit_ceil(sizes.accelerationStructureSize);desc.label="world_ray_scene";
    diagnostic.bytes=desc.size;
    if(!next->tlas || next->tlas->getDesc().size<sizes.accelerationStructureSize) {
      if(!diagnostic.check("tlas_create",r.device->createAccelerationStructure(desc,next->tlas.writeRef())))return false;
      ++stats.tlas_allocations;
    }
    if(!buffer(r,std::max(sizes.scratchSize,sizes.updateScratchSize),4,rhi::BufferUsage::UnorderedAccess,rhi::ResourceState::UnorderedAccess,frame.scratch,
         &diagnostic,"tlas_scratch"))return false;
    // TLAS references BLAS through device addresses, invisible to automatic tracking.
    commands->globalBarrier();
    // The exclusively owned frame slot has completed its fence. Full rebuilds
    // may reuse its capacity; shared or in-flight snapshots stay immutable.
    commands->buildAccelerationStructure(build,next->tlas,nullptr,frame.scratch,0,nullptr);
    commands->globalBarrier();
    current=next;frame.snapshot=std::move(next);
    frame.timing.end(commands);
    ++stats.tlas_builds;
    bytes_dirty=true;
    return true;
  }
}
