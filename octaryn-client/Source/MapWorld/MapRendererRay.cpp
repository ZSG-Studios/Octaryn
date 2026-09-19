#include "MapRendererInternal.h"
#include <slang-rhi/acceleration-structure-utils.h>
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace octaryn::client::rendering {
// 'MAP' TLAS instance user id; WorldRayQuery branches triangle hits on it.
static constexpr std::uint32_t map_ray_instance_id=0x4D4150u;
bool map_ray_ready(const MapRenderer& map) {return map.ray_ready && map.tlas && map.blas;}
rhi::IAccelerationStructure* map_ray_blas(const MapRenderer& map) {return map.blas.get();}
bool bind_map_ray_buffers(MapRenderer& map,rhi::IShaderObject* root) {
  if(!map.ray_ready)return true;
  rhi::ShaderCursor cursor(root);
  const auto bind=[&](const char* name,rhi::IBuffer* buffer) {
    auto field=cursor[name];
    return !field.isValid() || SLANG_SUCCEEDED(field.setBinding(rhi::Binding(buffer)));
  };
  return bind("mapVertices",map.vertices.get()) && bind("mapIndices",map.indices.get()) &&
      bind("mapRayPrimitives",map.ray_primitives.get()) &&
      bind("mapRayTrianglePrimitives",map.ray_triangle_primitives.get());
}
bool prepare_map_ray_scene(MapRenderer& map,rhi::ICommandEncoder* commands) {
  // Nothing to do once built or when the device lacks acceleration structures.
  if(map.ray_ready || !map.ray_supported)return true;
  if(!commands)return false;
  const auto fail=[](const char* stage) {
    std::fprintf(stderr,"map_ray_prepare_failed stage=%s\n",stage);
    return false;
  };
  // One BLAS over the whole world-space soup; PlayerShadows build pattern.
  rhi::AccelerationStructureBuildInput triangles{};
  triangles.type=rhi::AccelerationStructureBuildInputType::Triangles;
  auto& mesh=triangles.triangles;
  mesh.vertexBuffers[0]=map.vertices;mesh.vertexBufferCount=1;
  mesh.vertexFormat=rhi::Format::RGB32Float;
  mesh.vertexCount=static_cast<unsigned>(map.model.vertices.size());
  mesh.vertexStride=sizeof(MapVertex);
  mesh.indexBuffer=map.indices;mesh.indexFormat=rhi::IndexFormat::Uint32;
  mesh.indexCount=static_cast<unsigned>(map.model.indices.size());
  mesh.flags=rhi::AccelerationStructureGeometryFlags::Opaque;
  rhi::AccelerationStructureBuildDesc build{};
  build.inputs=&triangles;build.inputCount=1;
  build.flags=rhi::AccelerationStructureBuildFlags::PreferFastBuild;
  rhi::AccelerationStructureSizes sizes{};
  if(SLANG_FAILED(map.device->getAccelerationStructureSizes(build,&sizes)) ||
      !sizes.accelerationStructureSize)return fail("blas_sizes");
  rhi::AccelerationStructureDesc allocation{};
  allocation.kind=rhi::AccelerationStructureKind::BottomLevel;
  allocation.size=sizes.accelerationStructureSize;allocation.label="map_ray_blas";
  if(SLANG_FAILED(map.device->createAccelerationStructure(allocation,map.blas.writeRef())))return fail("blas_alloc");
  rhi::BufferDesc scratch{};
  scratch.size=sizes.scratchSize;scratch.elementSize=4;
  scratch.usage=rhi::BufferUsage::UnorderedAccess;
  scratch.defaultState=rhi::ResourceState::UnorderedAccess;
  if(SLANG_FAILED(map.device->createBuffer(scratch,nullptr,map.blas_scratch.writeRef())))return fail("blas_scratch");
  rhi::AccelerationStructureInstanceDescGeneric instance{};
  instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
  instance.instanceID=map_ray_instance_id;instance.instanceMask=255;
  instance.accelerationStructure=map.blas->getHandle();
  const auto type=rhi::getAccelerationStructureInstanceDescType(map.device.get());
  const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
  std::vector<std::uint8_t> native(stride);
  rhi::convertAccelerationStructureInstanceDescs(1,type,native.data(),stride,&instance,sizeof(instance));
  rhi::BufferDesc instances{};
  instances.size=stride;instances.elementSize=static_cast<unsigned>(stride);
  instances.usage=rhi::BufferUsage::AccelerationStructureBuildInput|rhi::BufferUsage::CopyDestination;
  instances.defaultState=rhi::ResourceState::AccelerationStructureBuildInput;
  if(SLANG_FAILED(map.device->createBuffer(instances,nullptr,map.instances.writeRef())) ||
      SLANG_FAILED(commands->uploadBufferData(map.instances,0,stride,native.data())))return fail("instances_upload");
  rhi::AccelerationStructureBuildInput scene_input{};
  scene_input.type=rhi::AccelerationStructureBuildInputType::Instances;
  scene_input.instances.instanceBuffer=map.instances;
  scene_input.instances.instanceStride=static_cast<unsigned>(stride);
  scene_input.instances.instanceCount=1;
  rhi::AccelerationStructureBuildDesc scene{};
  scene.inputs=&scene_input;scene.inputCount=1;
  scene.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
  rhi::AccelerationStructureSizes scene_sizes{};
  if(SLANG_FAILED(map.device->getAccelerationStructureSizes(scene,&scene_sizes)) ||
      !scene_sizes.accelerationStructureSize)return fail("tlas_sizes");
  rhi::AccelerationStructureDesc scene_allocation{};
  scene_allocation.kind=rhi::AccelerationStructureKind::TopLevel;
  scene_allocation.size=scene_sizes.accelerationStructureSize;scene_allocation.label="map_ray_scene";
  if(SLANG_FAILED(map.device->createAccelerationStructure(scene_allocation,map.tlas.writeRef())))return fail("tlas_alloc");
  scratch.size=std::max(scene_sizes.scratchSize,scene_sizes.updateScratchSize);
  if(SLANG_FAILED(map.device->createBuffer(scratch,nullptr,map.tlas_scratch.writeRef())))return fail("tlas_scratch");
  // Static geometry: the BLAS inputs stay in place for the whole session.
  commands->setBufferState(map.vertices,rhi::ResourceState::AccelerationStructureBuildInput);
  commands->setBufferState(map.indices,rhi::ResourceState::AccelerationStructureBuildInput);
  commands->globalBarrier();
  commands->buildAccelerationStructure(build,map.blas,nullptr,map.blas_scratch,0,nullptr);
  commands->globalBarrier();
  commands->buildAccelerationStructure(scene,map.tlas,nullptr,map.tlas_scratch,0,nullptr);
  commands->globalBarrier();
  commands->setBufferState(map.vertices,rhi::ResourceState::ShaderResource);
  commands->setBufferState(map.indices,rhi::ResourceState::ShaderResource);
  map.ray_ready=true;
  return true;
}
}
