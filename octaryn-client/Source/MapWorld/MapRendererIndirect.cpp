#include "MapRendererInternal.h"
#include "MapVisibility.h"
#include "CameraMatrix.h"
#include "WorldRendererInternal.h"
#include "MapLodCache.h"
#include "MapIndirectData.h"
#include "MapTextureCache.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace octaryn::client::rendering {
::camera map_visibility_camera(const WorldCamera& eye,const WorldRenderer& r) {
  ::camera visibility{};
  camera_init(&visibility,CAMERA_PROJECTION_PERSPECTIVE);
  visibility.position[0]=eye.x;visibility.position[1]=eye.y;visibility.position[2]=eye.z;
  visibility.yaw_radians=eye.yaw;visibility.pitch_radians=eye.pitch;
  visibility.vertical_field_of_view_radians=eye.vertical_fov;visibility.far_plane=8192;
  camera_resize(&visibility,r.render_width(),r.render_height());camera_update(&visibility);
  if(eye.jitter_x!=0 || eye.jitter_y!=0) {
    visibility.projection[0][0]/=1+std::abs(eye.jitter_x);
    visibility.projection[1][1]/=1+std::abs(eye.jitter_y);
    float relative_view[4][4],relative_projection[4][4];
    std::memcpy(relative_view,visibility.view,sizeof(relative_view));
    relative_view[3][0]=relative_view[3][1]=relative_view[3][2]=0;
    camera_matrix_multiply(relative_projection,visibility.projection,relative_view);
    camera_matrix_extract_frustum(visibility.relative_frustum_planes,relative_projection);
  }
  return visibility;
}
bool prepare_map_indirect_data(MapRenderer& map,const MapLodData& lods,float pixels,std::vector<MapIndirectPrimitive>& primitives) {
  const auto* mode=std::getenv("OCTARYN_CLIENT_MAP_DRAW_MODE");
  map.indirect_enabled=mode && std::strcmp(mode,"indirect")==0;
  if(!map.indirect_enabled)return true;
  // Opt-in: per-map phase-2 dispatch overhead exceeds the culling win at
  // Bistro tile granularity (see docs/development/map-asset-performance.md).
  const auto* occlusion=std::getenv("OCTARYN_CLIENT_MAP_OCCLUSION");
  map.occlusion_enabled=occlusion&&std::strcmp(occlusion,"1")==0;
  if(!map.device->hasFeature(rhi::Feature::MultiDrawIndirect) ||
      !map.device->hasFeature(rhi::Feature::DrawIndirectFirstInstance) ||
      map.model.primitives.size()>map.device->getInfo().limits.maxDrawIndirectCount) {
    std::fprintf(stderr,"map_indirect_failed reason=unsupported_indirect_draw\n");return false;
  }
  map.lod_pixel_error=pixels;
  if(pixels>0 && lods.primitives.size()!=map.model.primitives.size())return false;
  primitives.reserve(map.model.primitives.size());
  for(const auto& source:map.model.primitives) {
    MapIndirectPrimitive primitive{};
    for(unsigned i=0;i<3;++i) {primitive.minimum[i]=source.bounds_min[i];primitive.maximum[i]=source.bounds_max[i];}
    primitive.first=source.first_index;primitive.count=source.index_count;
    primitive.material=static_cast<unsigned>(primitives.size());primitive.opaque=source.material.alpha_mode!=MapAlphaMode::Blend;
    if(pixels>0)for(unsigned level=0;level<2;++level) {
      const auto& lod=lods.primitives[primitive.material][level];
      primitive.lods[level*2]=static_cast<unsigned>(map.model.indices.size())+lod.first;
      primitive.lods[level*2+1]=lod.count;primitive.errors[level]=lod.error;
    }
    primitives.push_back(primitive);
  }
  return true;
}
bool create_map_indirect_buffers(MapRenderer& map) {
  const auto* mode=std::getenv("OCTARYN_CLIENT_MAP_DRAW_MODE");
  if(!mode || std::strcmp(mode,"indirect")!=0)return true;
  MapLodData lods;float pixels=0;
  if(const auto* value=std::getenv("OCTARYN_CLIENT_MAP_LOD_PIXELS")) {
    char* end=nullptr;pixels=std::strtof(value,&end);
    if(end==value || *end || !std::isfinite(pixels) || pixels<0 || pixels>4)return false;
  }
  if(pixels>0) {
    auto source=map.texture_cache_directory;source.replace_extension();auto cache=source;cache+=".lods";
    std::string error;const auto hash=map_texture_file_digest(source,error);
    if(hash.empty() || !read_map_lods(cache,hash,map.model,lods,error)) {
      std::fprintf(stderr,"map_lod_failed reason=%s\n",error.c_str());return false;
    }
    std::vector<std::uint32_t> indices=map.model.indices;
    indices.insert(indices.end(),lods.indices.begin(),lods.indices.end());
    rhi::BufferDesc desc{};desc.size=indices.size()*4;desc.elementSize=4;
    desc.usage=rhi::BufferUsage::IndexBuffer;desc.defaultState=rhi::ResourceState::IndexBuffer;
    if(SLANG_FAILED(map.device->createBuffer(desc,indices.data(),map.lod_indices.writeRef())))return false;
  }
  std::vector<MapIndirectPrimitive> primitives;
  if(!prepare_map_indirect_data(map,lods,pixels,primitives))return false;
  rhi::BufferDesc desc{};desc.size=primitives.size()*sizeof(MapIndirectPrimitive);desc.elementSize=sizeof(MapIndirectPrimitive);
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopySource;desc.defaultState=rhi::ResourceState::ShaderResource;
  if(SLANG_FAILED(map.device->createBuffer(desc,primitives.data(),map.indirect_primitives.writeRef())))return false;
  std::printf("map_draw_mode indirect=1 primitives=%zu lod_pixel_error=%.3f lod_index_bytes=%zu occlusion=%u\n",
      primitives.size(),map.lod_pixel_error,lods.indices.size()*4,map.occlusion_enabled?1u:0u);return true;
}
}
