#include "MapRendererInternal.h"
#include "MapRayResources.h"
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering;
int main() {
  try {
    const auto require=[](bool value) {if(!value)throw std::runtime_error("ray input identity failed");};
    MapRenderer map;map.vertex_count=17;map.index_count=18;
    for(unsigned index=0;index<3;++index) {
      MapPrimitive primitive;primitive.first_index=index*6;primitive.index_count=6;
      primitive.material.alpha_mode=MapAlphaMode(index);map.model.primitives.push_back(primitive);
    }
    std::vector<rhi::AccelerationStructureBuildInput> inputs;
    require(map_ray_inputs(map,inputs) && inputs.size()==3);
    for(unsigned i=0;i<3;++i) {
      const auto& input=inputs[i].triangles;
      require(input.vertexCount==17 && input.vertexStride==80 && input.indexCount==6);
      require(input.indexBuffer.offset==i*24 && input.indexFormat==rhi::IndexFormat::Uint32);
      require(input.flags==(i==0?rhi::AccelerationStructureGeometryFlags::Opaque:rhi::AccelerationStructureGeometryFlags::None));
    }
    // CPU vertices were already released; retained counts remain the build source.
    require(map.model.vertices.empty() && map.model.indices.empty());
    for(const unsigned first:{1u,19u,UINT32_MAX}) {
      map.model.primitives[0].first_index=first;inputs.clear();require(!map_ray_inputs(map,inputs));
    }
    map.model.primitives[0].first_index=0;map.model.primitives[0].index_count=21;
    inputs.clear();require(!map_ray_inputs(map,inputs));
    std::puts("map_ray_input_tests passed=1 geometry_material_order=1 alpha_flags=1 retained_counts=1 invalid_ranges=4");
    return 0;
  } catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
