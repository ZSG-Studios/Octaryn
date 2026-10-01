#include "WorldRayTracingState.h"
#include "../../MapWorld/MapRendererInternal.h"
#include "TileBudget.h"
#include "WorldRaySceneAdmission.h"
#include <cstdio>
#include <slang-rhi/acceleration-structure-utils.h>
namespace octaryn::client::rendering {
bool WorldRayTracing::State::empty_blas(WorldRenderer& r,rhi::ICommandEncoder* commands,Frame& frame) {
    if(dummy)return true;
    RayPrepareDiagnostics diagnostic{"empty_blas"};diagnostic.frame=r.frames;diagnostic.generation=generation;
    const float box[6]={0,0,0,1,1,1};
    if(r.scene_memory && (!frame.scene_allocation ||
        !frame.scene_allocation->resize(frame.scene_allocation->bytes()+sizeof(box))))return false;
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
    if(r.scene_memory) {
      if(!scene_fixed_allocation || !scene_fixed_allocation->resize(desc.size) ||
          !frame.scene_allocation->resize(frame.scene_allocation->bytes()+
              std::bit_ceil((std::max<std::uint64_t>(sizes.scratchSize,4)+3)/4)*4))return false;
    }
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
      clear_snapshot_owners(*reusable);
      const auto capacity=[](const Snapshot& scene) {
        return (scene.tlas?scene.tlas->getDesc().size:0)+(scene.records?scene.records->getDesc().size:0);
      };
      if(!spare || capacity(*reusable)>capacity(*spare))spare=std::move(reusable);
      bytes_dirty=true;
    }
    bool same_maps=current && current->maps==r.resident_maps && current->map_blas.size()==r.resident_maps.size() &&
        current->geometry_snapshots.size()==r.resident_maps.size() && current->map_instance_revisions.size()==r.resident_maps.size();
    for(std::size_t i=0;same_maps && i<r.resident_maps.size();++i) {
      const auto& map=*r.resident_maps[i];
      same_maps=map_ray_ready(map) && current->map_blas[i].get()==map_ray_blas(map) &&
          current->map_instance_revisions[i]==map.geometry_instances_revision &&
          current->geometry_snapshots[i]==(map.geometry_ray?map.geometry_ray->snapshot():nullptr);
    }
    if(current && current->generation==generation && same_maps && current->item_revision==r.items.revision) {
      world_ray_scene_admission_complete(r);
      frame.snapshot=current;return true;
    }
    if(r.scene_memory && world_ray_admit_scene(r,r.resident_maps)!=SceneRayAdmission::Ready)return false;
    RayPrepareDiagnostics diagnostic{"snapshot"};diagnostic.frame=r.frames;diagnostic.generation=generation;
    if(!frame.timing.begin(r.device,commands,r.capabilities.timestamps,&diagnostic))return false;
    auto next=prewarm_items?exclusive_snapshot(snapshot_pool):
        spare && spare.use_count()==1?std::move(spare):std::make_shared<Snapshot>();
    if(!diagnostic.require("exclusive_snapshot_capacity",bool(next)))return false;
    next->generation=generation;next->map_blas.clear();next->maps.clear();next->geometry_snapshots.clear();next->columns.clear();
    next->map_instance_revisions.clear();
    next->item_assets.clear();next->item_revision=r.items.revision;
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
    auto& map_records=snapshot_map_records;map_records.clear();
    if(!diagnostic.require("map_instance_count",r.resident_maps.size()<0x800000u))return false;
    for(const auto& map:r.resident_maps) {
      if(!map_ray_ready(*map))continue;
      const auto clustered=map->geometry_ray?map->geometry_ray->snapshot():nullptr;
      if(clustered) {
        const auto instance_count=std::max<std::size_t>(1,map->geometry_instances.size());
        if(!diagnostic.require("cluster_instance_count",map_records.size()+clustered->batches.size()*instance_count<0x800000u))return false;
        for(std::size_t node=0;node<instance_count;++node)for(std::size_t batch=0;batch<clustered->batches.size();++batch) {
          MapRayGeometry geometry{};geometry.first_triangle=clustered->batches[batch].first_triangle;geometry.clustered=1;
          geometry.vertex_stride=clustered->vertex_stride;
          if(!descriptor(clustered->vertices,geometry.vertices,diagnostic,"cluster_vertices") ||
              !descriptor(clustered->indices,geometry.indices,diagnostic,"cluster_indices") ||
              !descriptor(clustered->triangle_records,geometry.triangles,diagnostic,"cluster_triangles") ||
              !descriptor(map->ray_primitives,geometry.materials,diagnostic,"cluster_materials",map->material_buffer_range))return false;
          rhi::AccelerationStructureInstanceDescGeneric instance{};
          instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
          if(!map->geometry_instances.empty()) {
            const auto& transform=map->geometry_instances[node];geometry.clustered|=2;
            geometry.world=transform.world;geometry.normal=transform.normal;geometry.orientation=transform.orientation;
            for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column)
              instance.transform[row][column]=transform.world[row*4+column];
          }
          instance.instanceID=0x800000u|static_cast<std::uint32_t>(map_records.size());instance.instanceMask=0xFF;
          instance.flags=rhi::AccelerationStructureInstanceFlags::TriangleFacingCullDisable;
          instance.accelerationStructure=clustered->blas[batch]->getHandle();
          generic.push_back(instance);map_records.push_back(geometry);
        }
        next->maps.push_back(map);next->map_blas.emplace_back(clustered->blas.front());
        next->map_instance_revisions.push_back(map->geometry_instances_revision);
        next->geometry_snapshots.push_back(clustered);continue;
      }
      MapRayGeometry geometry{};
      if(!map_ray_geometry(*map,geometry))return false;
      rhi::AccelerationStructureInstanceDescGeneric instance{};
      instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
      instance.instanceID=0x800000u|static_cast<std::uint32_t>(map_records.size());instance.instanceMask=0xFF;
      auto* blas=map_ray_blas(*map);instance.accelerationStructure=blas->getHandle();
      generic.push_back(instance);map_records.push_back(geometry);
      next->maps.push_back(map);next->map_blas.emplace_back(blas);
      next->map_instance_revisions.push_back(map->geometry_instances_revision);
      next->geometry_snapshots.emplace_back();
    }
    next->static_instance_count=static_cast<std::uint32_t>(map_records.size());
    if(!r.items.instances.empty()) {
      if(prewarm_items && r.items.instances.size()>std::max(prewarm_items,reported_item_growth)) {
        std::printf("world_ray_capacity_growth items=%zu prewarmed=%u maximum=%u\n",
            r.items.instances.size(),prewarm_items,MaximumItemCapacity);
        reported_item_growth=unsigned(r.items.instances.size());
      }
      const auto first_item=static_cast<std::uint32_t>(map_records.size());
      if(!diagnostic.require("item_instance_count",map_records.size()+r.items.assets.size()<0x800000u))return false;
      for(const auto& asset:r.items.assets) {
        MapRayGeometry geometry{};
        if(!map_ray_ready(*asset.mesh) || !map_ray_geometry(*asset.mesh,geometry))return false;
        map_records.push_back(geometry);next->item_assets.push_back(asset.mesh);
      }
      for(const auto& batch:r.items.batches)for(unsigned i=0;i<batch.count;++i) {
        const auto& position=r.items.instances[batch.first+i].current;
        rhi::AccelerationStructureInstanceDescGeneric instance{};
        instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
        for(unsigned axis=0;axis<3;++axis)instance.transform[axis][3]=position[axis];
        instance.instanceID=0x800000u|(first_item+batch.asset);instance.instanceMask=0xFF;
        instance.accelerationStructure=map_ray_blas(*r.items.assets[batch.asset].mesh)->getHandle();
        generic.push_back(instance);
      }
    }
    if(map_records.empty())map_records.emplace_back();
    if(records.empty()) {
      records.emplace_back();
      if(next->maps.empty()) {
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
    // Queries only need a valid address. Measure the complete TLAS growth before
    // allocating its node descriptors, native instances, or acceleration data.
    if(!frame.instances) {
      if(r.scene_memory && (!frame.scene_allocation ||
          !frame.scene_allocation->resize(frame.scene_allocation->bytes()+stride)))return false;
      if(!buffer(r,stride,static_cast<unsigned>(stride),rhi::BufferUsage::AccelerationStructureBuildInput,
          rhi::ResourceState::AccelerationStructureBuildInput,frame.instances,&diagnostic,"size_query_address"))return false;
    }
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
    input.instances.instanceBuffer=frame.instances;input.instances.instanceStride=static_cast<unsigned>(stride);
    input.instances.instanceCount=static_cast<std::uint32_t>(generic.size());
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
    rhi::AccelerationStructureSizes sizes{};
    if(!diagnostic.check("tlas_sizes",r.device->getAccelerationStructureSizes(build,&sizes)) ||
       !diagnostic.require("tlas_size_nonzero",sizes.accelerationStructureSize!=0))return false;
    std::uint64_t allocation_bytes{},scene_capacity{},frame_capacity{};
    const auto capacity=[&](rhi::IBuffer* existing,std::uint64_t bytes,std::uint64_t element) {
      const auto required=std::bit_ceil((std::max(bytes,element)+element-1)/element)*element;
      if(existing && existing->getDesc().size>=bytes)return existing->getDesc().size;
      allocation_bytes+=required;return required;
    };
    scene_capacity+=capacity(next->map_records,map_records.size()*sizeof(MapRayGeometry),sizeof(MapRayGeometry));
    scene_capacity+=capacity(next->records,record_bytes,sizeof(Record));
    frame_capacity+=capacity(frame.instances,native.size(),stride);
    frame_capacity+=capacity(frame.scratch,std::max(sizes.scratchSize,sizes.updateScratchSize),4);
    if(frame.dummy_bounds)frame_capacity+=frame.dummy_bounds->getDesc().size;
    if(frame.dummy_scratch)frame_capacity+=frame.dummy_scratch->getDesc().size;
    if(next->tlas && next->tlas->getDesc().size>=sizes.accelerationStructureSize)scene_capacity+=next->tlas->getDesc().size;
    else {const auto bytes=std::bit_ceil(sizes.accelerationStructureSize);scene_capacity+=bytes;allocation_bytes+=bytes;}
    // Reused capacities and every retained TLAS participate in the same scene
    // ledger as pages and BLAS. During growth the old and new capacities overlap.
    std::shared_ptr<virtual_geometry::SceneMemoryLease> old_scene,old_frame;
    if(r.scene_memory) {
      using namespace virtual_geometry;
      if(!next->scene_allocation || next->scene_allocation->bytes()<scene_capacity) {
        auto lease=world_ray_scene_credit(r,scene_capacity);
        if(!diagnostic.require("scene_tlas_aggregate_admission",bool(lease)))return false;
        old_scene=std::move(next->scene_allocation);next->scene_allocation=std::move(lease);
      }
      if(!frame.scene_allocation || frame.scene_allocation->bytes()<frame_capacity) {
        auto lease=world_ray_scene_credit(r,frame_capacity);
        if(!diagnostic.require("scene_tlas_frame_admission",bool(lease)))return false;
        old_frame=std::move(frame.scene_allocation);frame.scene_allocation=std::move(lease);
      }
    }
    if(allocation_bytes) {
      const auto memory=device_memory_stats(r.device->getInfo(),true);
      if(!diagnostic.require("scene_gpu_memory_admission",!memory.budget_available ||
          tile_gpu_admits(allocation_bytes,memory.local_budget,memory.local_usage,0,memory.reserved_capacity_bytes)))return false;
      next->capacity_reservation=std::make_shared<DeviceMemoryReservation>(scene_capacity);
      frame.capacity_reservation=std::make_shared<DeviceMemoryReservation>(frame_capacity);
    }
    if(!buffer(r,map_records.size()*sizeof(MapRayGeometry),sizeof(MapRayGeometry),
        rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination,rhi::ResourceState::ShaderResource,
        next->map_records,&diagnostic,"map_records_buffer") ||
        !diagnostic.check("map_records_upload",commands->uploadBufferData(next->map_records,0,
            map_records.size()*sizeof(MapRayGeometry),map_records.data())))return false;
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
    input.instances.instanceBuffer=frame.instances;
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
    if(current && current->scene_allocation)current->scene_allocation->phase(virtual_geometry::SceneMemoryPhase::Retired);
    if(next->scene_allocation)next->scene_allocation->phase(virtual_geometry::SceneMemoryPhase::Resident);
    current=next;frame.snapshot=std::move(next);
    world_ray_scene_admission_complete(r);
    frame.timing.end(commands);
    ++stats.tlas_builds;
    bytes_dirty=true;
    return true;
  }
}
