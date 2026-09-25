#include "WorldRendererInternal.h"
#include "BlockLights.h"
#include "BlockLightSelection.h"
#include <algorithm>
#include <cmath>
namespace octaryn::client::rendering {
void world_block_lights_store(WorldRenderer& r,const world_presentation::StreamColumn& source) {
  auto& state=r.block_lights;
  auto& column=state.columns[{source.x,source.z}];
  if(column.blocks.storage_identity()==source.blocks.storage_identity() &&
      column.min_y==source.min_y && column.height==source.height &&
      column.revision==source.revision &&
      column.authoritative_revision==source.authoritative_revision)return;
  state.source_count-=column.lights.size();column.lights.clear();
  column.blocks=source.blocks;column.min_y=source.min_y;column.height=source.height;
  column.revision=source.revision;column.authoritative_revision=source.authoritative_revision;
  const auto materials=world_atlas_emissions(r.atlas);
  const auto emits=[&](unsigned id) {return id<materials.size() && materials[id].radiance_range[3]>0;};
  source.blocks.visit_matching(emits,[&](std::size_t index,unsigned id) {
    const int x=int(index%32),y=int(index/32%std::size_t(source.height)),z=int(index/(32u*std::size_t(source.height)));
    const auto& material=materials[id];
    // Interior lava/solid emitters have no radiating surface. Column edges stay
    // candidates so neighbor arrival never hides a real source permanently.
    if(!material.sprite && x>0 && x<31 && y>0 && y<source.height-1 && z>0 && z<31) {
      const std::size_t steps[]={1,32,32u*std::size_t(source.height)};
      bool exposed=false;
      for(auto step:steps)for(int sign:{-1,1}) {
        const auto neighbor=source.blocks[std::size_t(std::ptrdiff_t(index)+sign*std::ptrdiff_t(step))];
        if(neighbor>=materials.size() || !materials[neighbor].occludes)exposed=true;
      }
      if(!exposed)return;
    }
    WorldLocalLight light;
    const auto& emission=material.radiance_range;
    light.position_range={float(source.x)*32+float(x)+.5f,float(source.min_y+y)+(material.sprite?.7f:.5f),float(source.z)*32+float(z)+.5f,emission[3]};
    light.color_intensity={emission[0],emission[1],emission[2],1};
    light.axis_v_type[3]=3;column.lights.push_back(light);
  });
  state.source_count+=column.lights.size();state.dirty=true;
}
void world_block_lights_remove(WorldRenderer& r,std::pair<int,int> coordinate) {
  auto& state=r.block_lights;const auto found=state.columns.find(coordinate);
  if(found==state.columns.end())return;
  state.source_count-=found->second.lights.size();state.columns.erase(found);state.dirty=true;
}
void world_block_lights_update(WorldRenderer& r) {
  auto& state=r.block_lights;
  const std::array<int,3> cell{int(std::floor(r.draw_uniforms[0]/8)),int(std::floor(r.draw_uniforms[1]/8)),int(std::floor(r.draw_uniforms[2]/8))};
  if(!state.dirty && cell==state.selected_cell)return;
  state.dirty=false;state.selected_cell=cell;
  const std::array<float,3> center{float(cell[0])*8+4.f,float(cell[1])*8+4.f,float(cell[2])*8+4.f};
  BlockLightSelection selection(center,state.explicit_lights.size());
  for(const auto& [coordinate,column]:state.columns)for(const auto& light:column.lights)selection.add(light);
  state.selected_count=unsigned(selection.selected_count());
  // Explicit API lights are retained unchanged; fog only affects presentation.
  auto lights=selection.finish(state.explicit_lights);
  auto& s=r.local_lighting;
  if(s.lights.size()==lights.size() && std::equal(s.lights.begin(),s.lights.end(),lights.begin(),world_local_light_equal))return;
  s.lights=std::move(lights);++s.light_revision;
}
}
