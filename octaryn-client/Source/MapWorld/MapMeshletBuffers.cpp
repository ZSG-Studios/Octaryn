#include "MapRendererInternal.h"
#include "MapMeshlets.h"
#include <cstdio>
namespace octaryn::client::rendering {
bool create_map_meshlet_buffers(MapRenderer& map) {
  map.meshlet_enabled=map_meshlet_requested();if(!map.meshlet_enabled)return true;
  if(!map.device->hasFeature(rhi::Feature::MeshShader)) {
    std::fprintf(stderr,"map_meshlet_failed reason=mesh_shader_unsupported\n");return false;
  }
  MapMeshletData data;std::string error;
  if(!prepare_map_meshlets(map.model,data,error)) {
    std::fprintf(stderr,"map_meshlet_failed reason=%s\n",error.c_str());return false;
  }
  map.meshlet_count=unsigned(data.records.size());if(!map.meshlet_count)return true;
  const auto upload=[&](const void* source,std::uint64_t bytes,unsigned stride,Slang::ComPtr<rhi::IBuffer>& target) {
    rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;
    desc.usage=rhi::BufferUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;
    return SLANG_SUCCEEDED(map.device->createBuffer(desc,source,target.writeRef()));
  };
  if(!upload(data.records.data(),data.records.size()*sizeof(MapMeshlet),sizeof(MapMeshlet),map.meshlets) ||
      !upload(data.vertices.data(),data.vertices.size()*4,4,map.meshlet_vertices) ||
      !upload(data.triangles.data(),data.triangles.size()*4,4,map.meshlet_triangles))return false;
  std::printf("map_draw_mode meshlet=1 meshlets=%u triangles=%zu bytes=%llu vertices_per_group=128 triangles_per_group=256\n",
      map.meshlet_count,data.triangles.size(),static_cast<unsigned long long>(data.bytes()));
  return true;
}
}
