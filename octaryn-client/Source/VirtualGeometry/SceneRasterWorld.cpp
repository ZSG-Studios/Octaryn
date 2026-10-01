#include "WorldGeometryRaster.h"
#include "WorldGeometry.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include "../MapWorld/MapRendererInternal.h"
#include <cstdio>

namespace octaryn::client::rendering::virtual_geometry {
bool WorldGeometryRaster::scene_visibility(WorldRenderer& renderer,rhi::ICommandEncoder* commands,const WorldCamera& camera) {
  scene_recorded=false;if(renderer.resident_maps.empty())return true;
  if(!scene_tables || !scene_initialized) {renderer.status="scene raster is not initialized";return false;}
  std::vector<SceneRasterAsset> assets;assets.reserve(renderer.resident_maps.size());
  const auto* first=renderer.resident_maps.front().get();
  auto* pool=first->geometry->stream().pool();auto* materials=first->ray_primitives.get();
  for(const auto& map:renderer.resident_maps) {
    if(!map->geometry || !map->geometry->ready() || map->geometry->stream().pool()!=pool || map->ray_primitives.get()!=materials ||
        map->material_buffer_range.offset%sizeof(MapRayMaterial)) {renderer.status="scene raster bindings differ or are incomplete";return false;}
    const auto& geometry=map->geometry->asset();
    assets.push_back({map->geometry->stream().clusters(),unsigned(geometry.clusters.size()),unsigned(geometry.pages.size()),
        unsigned(map->material_buffer_range.offset/sizeof(MapRayMaterial)),map->geometry_instances});
  }
  SceneRasterFrame frame;
  if(!scene_tables->begin(commands,renderer.active_frame,assets,frame)) {renderer.status=scene_tables->error();return false;}
  unsigned root_domains{};
  for(unsigned i=0;i<renderer.resident_maps.size();++i) {
    auto& geometry=*renderer.resident_maps[i]->geometry;
    if(geometry.complete_root_cut()) {
      if(!geometry.stage_uploads(commands)) {renderer.status=geometry.error();return false;}
      if(!scene_tables->append_roots(commands,i,geometry.stream().page_table())) {renderer.status=scene_tables->error();return false;}
      ++root_domains;
    }else {
      if(!geometry.prepare(renderer,commands,camera,0,true)) {renderer.status=geometry.error();return false;}
      if(!scene_tables->append(commands,i,geometry.selection_frame())) {renderer.status=scene_tables->error();return false;}
    }
  }
  if(!scene_tables->finish(commands)) {renderer.status=scene_tables->error();return false;}
  auto& input=scene_inputs[renderer.active_frame];
  input={frame.clusters,pool,frame.pages,frame.selected,frame.counters,materials,frame.dispatch,
      unsigned(renderer.render_width()),unsigned(renderer.render_height()),frame.capacity,unsigned(pool->getDesc().size/page_bytes),
      renderer.view_uniforms,{renderer.lighting.skylight_floor,renderer.lighting.gameplay_sky_visibility,0,0}};
  input.scene_draws=frame.draws;input.scene_instances=frame.instances;input.scene_frame=frame.generation;
  auto& raster=scene_frames[renderer.active_frame];
  if(!raster.resize(renderer.device,input.width,input.height) || !raster.visibility(commands,input)) {
    renderer.status="scene raster visibility failed";return false;
  }
  scene_recorded=true;
  if(renderer.frames%120==0)std::printf("scene_raster frame=%llu assets=%zu root_domains=%u selection_domains=%zu draw_capacity=%u table_bytes=%llu visibility_passes=1 material_passes=1\n",
      static_cast<unsigned long long>(renderer.frames),assets.size(),root_domains,assets.size()-root_domains,frame.capacity,
      static_cast<unsigned long long>(scene_tables->gpu_bytes()));
  return true;
}
bool WorldGeometryRaster::scene_resolve(WorldRenderer& renderer,rhi::IRenderPassEncoder* pass) {
  return !scene_recorded || scene_frames[renderer.active_frame].resolve(pass,scene_inputs[renderer.active_frame]);
}
bool WorldGeometryRaster::submitted(rhi::IFence* fence,std::uint64_t value) {
  if(!scene_tables || !scene_recorded)return true;
  if(!scene_tables->submitted(fence,value))return false;scene_recorded=false;return true;
}
}
