#include "MapRayResources.h"
#include "MapRendererInternal.h"
#include <slang-rhi/acceleration-structure-utils.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
namespace octaryn::client::rendering {
bool map_ray_inputs(MapRenderer& map,std::vector<rhi::AccelerationStructureBuildInput>& inputs) {
  // Geometry index matches material index; opaque meshes commit in traversal.
  inputs.reserve(map.model.primitives.size());
  for(const auto& primitive:map.model.primitives) {
    if(primitive.first_index%3 || primitive.index_count%3 ||
        primitive.first_index>map.index_count ||
        primitive.index_count>map.index_count-primitive.first_index)return false;
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Triangles;
    auto& mesh=input.triangles;
    mesh.vertexBuffers[0]=map.vertices;mesh.vertexBufferCount=1;
    mesh.vertexFormat=rhi::Format::RGB32Float;
    mesh.vertexCount=static_cast<unsigned>(map.vertex_count);
    mesh.vertexStride=sizeof(MapVertex);
    mesh.indexBuffer={map.indices.get(),rhi::Offset(primitive.first_index)*sizeof(std::uint32_t)};
    mesh.indexFormat=rhi::IndexFormat::Uint32;mesh.indexCount=primitive.index_count;
    mesh.flags=primitive.material.alpha_mode==MapAlphaMode::Opaque?
        rhi::AccelerationStructureGeometryFlags::Opaque:rhi::AccelerationStructureGeometryFlags::None;
    inputs.push_back(input);
  }
  return true;
}
bool allocate_map_ray_resources(MapRenderer& map,const std::atomic_bool* cancel) {
  if(map.ray_resources_ready || !map.ray_supported)return true;
  const auto start=std::chrono::steady_clock::now();
  const auto fail=[](const char* stage) {
    std::fprintf(stderr,"map_ray_resource_failed stage=%s\n",stage);return false;
  };
  std::vector<rhi::AccelerationStructureBuildInput> inputs;
  if(!map_ray_inputs(map,inputs))return fail("primitive_range");
  rhi::AccelerationStructureBuildDesc build{};
  build.inputs=inputs.data();build.inputCount=static_cast<unsigned>(inputs.size());
  // Static maps amortize construction over the entire session.
  build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace|rhi::AccelerationStructureBuildFlags::AllowCompaction;
  rhi::AccelerationStructureSizes sizes{};
  if(cancel && cancel->load(std::memory_order_relaxed))return false;
  if(SLANG_FAILED(map.device->getAccelerationStructureSizes(build,&sizes)) ||
      !sizes.accelerationStructureSize)return fail("blas_sizes");
  std::printf("map_ray_allocation geometries=%zu triangles=%u blas_bytes=%llu scratch_bytes=%llu\n",
      inputs.size(),map.index_count/3,
      static_cast<unsigned long long>(sizes.accelerationStructureSize),
      static_cast<unsigned long long>(sizes.scratchSize));
  std::fflush(stdout);
  rhi::AccelerationStructureDesc allocation{};
  allocation.kind=rhi::AccelerationStructureKind::BottomLevel;
  allocation.size=sizes.accelerationStructureSize;allocation.label="map_ray_blas";
  if(cancel && cancel->load(std::memory_order_relaxed))return false;
  if(SLANG_FAILED(map.device->createAccelerationStructure(allocation,map.blas.writeRef())))return fail("blas_alloc");
  rhi::BufferDesc scratch{};
  scratch.size=sizes.scratchSize;scratch.elementSize=4;
  scratch.usage=rhi::BufferUsage::UnorderedAccess;
  scratch.defaultState=rhi::ResourceState::UnorderedAccess;
  if(cancel && cancel->load(std::memory_order_relaxed))return false;
  if(SLANG_FAILED(map.device->createBuffer(scratch,nullptr,map.blas_scratch.writeRef())))return fail("blas_scratch");
  const auto type=rhi::getAccelerationStructureInstanceDescType(map.device.get());
  const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
  rhi::BufferDesc instances{};
  instances.size=stride;instances.elementSize=static_cast<unsigned>(stride);
  instances.usage=rhi::BufferUsage::AccelerationStructureBuildInput|rhi::BufferUsage::CopyDestination;
  instances.defaultState=rhi::ResourceState::AccelerationStructureBuildInput;
  if(cancel && cancel->load(std::memory_order_relaxed))return false;
  if(SLANG_FAILED(map.device->createBuffer(instances,nullptr,map.instances.writeRef())))return fail("instances_alloc");
  rhi::AccelerationStructureBuildInput scene_input{};
  scene_input.type=rhi::AccelerationStructureBuildInputType::Instances;
  scene_input.instances.instanceBuffer=map.instances;
  scene_input.instances.instanceStride=static_cast<unsigned>(stride);
  scene_input.instances.instanceCount=1;
  rhi::AccelerationStructureBuildDesc scene{};
  scene.inputs=&scene_input;scene.inputCount=1;
  scene.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
  rhi::AccelerationStructureSizes scene_sizes{};
  if(cancel && cancel->load(std::memory_order_relaxed))return false;
  if(SLANG_FAILED(map.device->getAccelerationStructureSizes(scene,&scene_sizes)) ||
      !scene_sizes.accelerationStructureSize)return fail("tlas_sizes");
  rhi::AccelerationStructureDesc scene_allocation{};
  scene_allocation.kind=rhi::AccelerationStructureKind::TopLevel;
  scene_allocation.size=scene_sizes.accelerationStructureSize;scene_allocation.label="map_ray_scene";
  if(cancel && cancel->load(std::memory_order_relaxed))return false;
  if(SLANG_FAILED(map.device->createAccelerationStructure(scene_allocation,map.tlas.writeRef())))return fail("tlas_alloc");
  scratch.size=std::max(scene_sizes.scratchSize,scene_sizes.updateScratchSize);
  if(cancel && cancel->load(std::memory_order_relaxed))return false;
  if(SLANG_FAILED(map.device->createBuffer(scratch,nullptr,map.tlas_scratch.writeRef())))return fail("tlas_scratch");
  if(cancel && cancel->load(std::memory_order_relaxed))return false;
  rhi::QueryPoolDesc query{};query.type=rhi::QueryType::AccelerationStructureCompactedSize;
  query.count=1;query.label="map_blas_compact_size";
  if(SLANG_FAILED(map.device->createQueryPool(query,map.compact_size.writeRef())))return fail("compact_query");
  map.ray_resources_ready=true;
  std::printf("map_ray_resources cpu_ms=%.3f triangles=%u\n",
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),map.index_count/3);
  return true;
}
}
