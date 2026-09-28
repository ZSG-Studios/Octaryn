#include "MapRayBudget.h"
#include "MapRendererInternal.h"
#include <slang-rhi/acceleration-structure-utils.h>
#include <algorithm>
#include <vector>
namespace octaryn::client::rendering {
namespace {
bool query(rhi::IDevice* device,const MapModel& model,unsigned vertex_count,
    rhi::IBuffer* vertices,rhi::IBuffer* indices,std::uint64_t& peak_bytes) {
  peak_bytes=0;
  if(!device->hasFeature(rhi::Feature::AccelerationStructure))return true;
  if(!vertices || !indices || model.primitives.empty())return false;
  std::vector<rhi::AccelerationStructureBuildInput> inputs;
  for(const auto& primitive:model.primitives) {
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Triangles;
    auto& mesh=input.triangles;mesh.vertexBuffers[0]=vertices;mesh.vertexBufferCount=1;
    mesh.vertexFormat=rhi::Format::RGB32Float;mesh.vertexCount=vertex_count;mesh.vertexStride=sizeof(MapVertex);
    mesh.indexBuffer={indices,0};
    mesh.indexFormat=rhi::IndexFormat::Uint32;mesh.indexCount=primitive.index_count;
    mesh.flags=primitive.material.alpha_mode==MapAlphaMode::Opaque?
        rhi::AccelerationStructureGeometryFlags::Opaque:rhi::AccelerationStructureGeometryFlags::None;
    inputs.push_back(input);
  }
  rhi::AccelerationStructureBuildDesc build{};build.inputs=inputs.data();build.inputCount=unsigned(inputs.size());
  build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace|rhi::AccelerationStructureBuildFlags::AllowCompaction;
  rhi::AccelerationStructureSizes blas{},tlas{};
  if(SLANG_FAILED(device->getAccelerationStructureSizes(build,&blas)) || !blas.accelerationStructureSize)return false;
  const auto stride=rhi::getAccelerationStructureInstanceDescSize(rhi::getAccelerationStructureInstanceDescType(device));
  rhi::AccelerationStructureBuildInput scene{};scene.type=rhi::AccelerationStructureBuildInputType::Instances;
  // The RHI converter requires an address for a size query; no command reads it.
  scene.instances.instanceBuffer=vertices;scene.instances.instanceCount=1;scene.instances.instanceStride=unsigned(stride);
  build.inputs=&scene;build.inputCount=1;build.flags=rhi::AccelerationStructureBuildFlags::PreferFastTrace;
  if(SLANG_FAILED(device->getAccelerationStructureSizes(build,&tlas)) || !tlas.accelerationStructureSize)return false;
  // Compaction releases BLAS scratch first and only keeps compact results smaller
  // than the source. Two uncompressed sizes safely bound that transient phase.
  peak_bytes=std::max(blas.accelerationStructureSize+blas.scratchSize,2*blas.accelerationStructureSize)+
      tlas.accelerationStructureSize+std::max(tlas.scratchSize,tlas.updateScratchSize)+stride;
  return true;
}
}
bool map_ray_build_budget(const MapRenderer& map,std::uint64_t& peak_bytes) {
  return query(map.device,map.model,map.vertex_count,map.vertices,map.indices,peak_bytes);
}
bool map_ray_prepare_budget(rhi::IDevice* device,rhi::IBuffer* address,const MapModel& model,std::uint64_t& peak_bytes) {
  return query(device,model,unsigned(model.vertices.size()),address,address,peak_bytes);
}
}
