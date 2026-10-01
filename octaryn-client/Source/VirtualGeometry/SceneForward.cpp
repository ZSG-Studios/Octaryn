#include "SceneForward.h"
#include "../MapWorld/MapAssetBuildInternal.h"
#include "../Rendering/RenderBackend/SlangShaderPath.h"

namespace octaryn::client::rendering {
bool upload_scene_forward(MapRenderer& map,std::string& error) {
  if(map.model.primitives.size()!=1 || map.model.primitives[0].material.alpha_mode!=MapAlphaMode::Blend ||
      map.model.indices.empty() || map.model.indices.size()>262144*3) {error="scene forward representation must be a bounded BLEND primitive";return false;}
  const auto buffer=[&](const void* source,std::size_t size,std::size_t stride,bool index,Slang::ComPtr<rhi::IBuffer>& output) {
    rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;
    desc.usage=index?rhi::BufferUsage::IndexBuffer:rhi::BufferUsage::ShaderResource;
    desc.defaultState=index?rhi::ResourceState::IndexBuffer:rhi::ResourceState::ShaderResource;
    return SLANG_SUCCEEDED(map.device->createBuffer(desc,source,output.writeRef()));
  };
  map.vertex_count=std::uint32_t(map.model.vertices.size());map.index_count=std::uint32_t(map.model.indices.size());
  if(!buffer(map.model.vertices.data(),map.model.vertices.size()*sizeof(MapVertex),sizeof(MapVertex),false,map.vertices) ||
      !buffer(map.model.indices.data(),map.model.indices.size()*4,4,false,map.indices) ||
      !buffer(map.model.indices.data(),map.model.indices.size()*4,4,true,map.raster_indices)) {
    error="scene forward geometry upload failed";return false;
  }
  map.any_double_sided_blend=map.model.primitives[0].material.double_sided;
  const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/Map/WorldMap.slang");
  if(shader.empty() || !create_map_pipelines(map,rhi::Format::RGBA16Float,rhi::Format::D32Float,shader.c_str())) {
    error="scene forward pipeline creation failed";return false;
  }
  return true;
}
}
