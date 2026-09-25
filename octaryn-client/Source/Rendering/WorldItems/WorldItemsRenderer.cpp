#include "WorldItemsRenderer.h"
#include "WorldItemsClient.h"
#include "WorldAtlas.h"
#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>
#include "RhiShader.h"
#include "../BlockTransportGI/DynamicReceivers.h"
#include "../BlockTransportGI/ReceiverGeometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <bit>

namespace octaryn::client::rendering {
namespace {
struct Record {float position[4]{};std::uint32_t material[4]{};};
struct View {float camera[4]{},right[4]{},up[4]{},forward[4]{},projection[4]{};};
View view(const WorldCamera& c,int width,int height) {
  const float sy=std::sin(c.yaw),cy=std::cos(c.yaw),sp=std::sin(c.pitch),cp=std::cos(c.pitch);
  const float focal=1/std::tan(std::clamp(c.vertical_fov,.2f,2.7f)/2);
  return {{c.x,c.y,c.z,0},{cy,0,sy,c.jitter_x},{-sy*sp,cp,cy*sp,c.jitter_y},
    {sy*cp,sp,-cy*cp,0},{focal*static_cast<float>(height)/static_cast<float>(width),focal,
    8192.f/(8192.f-.1f),.1f*8192.f/(8192.f-.1f)}};
}
struct Uniforms {View current,previous;float lighting[4],settings[4];
  Record items[256],previous_items[256];};
}
struct WorldItemsRenderer {
  Slang::ComPtr<rhi::IRenderPipeline> pipeline,temporal_pipeline;
  world_presentation::WorldItemSnapshot source;
  std::array<Record,256> starts{},displayed{},committed{};
  std::array<std::uint64_t,256> displayed_ids{},committed_ids{};
  std::size_t displayed_count{},committed_count{};
  double transition{},duration{1.0/30},animation{},committed_animation{};
  View current_view{},committed_view{};
  Uniforms uniforms{};
  Record provisional_record{};
  bool prepared_provisional{};
  DynamicReceivers indirect;
 bool history{};
 std::optional<world_presentation::ProvisionalToss> provisional;
};
WorldItemsRenderer* create_world_items_renderer(rhi::IDevice* device,rhi::Format color,
    rhi::Format depth,const char* shader_path) {
  if(!device||!shader_path)return nullptr;
  auto renderer=std::make_unique<WorldItemsRenderer>();
  rhi::ColorTargetDesc targets[2]{};targets[0].format=color;targets[1].format=rhi::Format::RGBA16Float;
  rhi::RenderPipelineDesc desc{};desc.targets=targets;desc.targetCount=1;
  desc.rasterizer.cullMode=rhi::CullMode::None;
  desc.depthStencil.format=depth;desc.depthStencil.depthTestEnable=true;
  desc.depthStencil.depthWriteEnable=true;desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  const char* entries[]{"vertex_main","fragment_main"};Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(device,shader_path,entries,2,program))return nullptr;
  desc.program=program;
  if(SLANG_FAILED(device->createRenderPipeline(desc,renderer->pipeline.writeRef())))return nullptr;
  entries[1]="temporal_fragment_main";
  if(!create_rhi_program(device,shader_path,entries,2,program))return nullptr;
  desc.program=program;desc.targetCount=2;
  if(SLANG_FAILED(device->createRenderPipeline(desc,renderer->temporal_pipeline.writeRef())))return nullptr;
  return renderer.release();
}
void destroy_world_items_renderer(WorldItemsRenderer* renderer){delete renderer;}
bool initialize_item_receivers(WorldItemsRenderer* renderer,WorldRenderer& world) {
  return !renderer || initialize_dynamic_receivers(renderer->indirect,world);
}
const DynamicReceivers* item_receiver_stats(const WorldItemsRenderer* renderer) {return renderer?&renderer->indirect:nullptr;}
void set_provisional_toss(WorldItemsRenderer* renderer,
 const std::optional<world_presentation::ProvisionalToss>& provisional) {
 if(renderer)renderer->provisional=provisional;
}
bool prepare_world_items_frame(WorldItemsRenderer* r,WorldRenderer& world,rhi::ICommandEncoder* commands,const WorldCamera& camera,
    int width,int height,WorldAtlas* atlas,const world_presentation::WorldItemSnapshot& snapshot,
    double elapsed,const PlayerLighting& lighting,bool reset,float mip_bias) {
  if(!r||!commands||!atlas||width<=0||height<=0||snapshot.items.size()>256||!std::isfinite(elapsed))return false;
  auto* materials=world_atlas_materials(atlas);
  if(!materials)return false;
  const auto& material_desc=materials->getDesc();
  if(!material_desc.elementSize||material_desc.size<material_desc.elementSize)return false;
  const auto material_count=material_desc.size/material_desc.elementSize;
  elapsed=std::clamp(elapsed,0.0,.1);r->animation+=elapsed;
  if(snapshot.source_seconds!=r->source.source_seconds||snapshot.items.size()!=r->source.items.size()) {
    r->duration=std::clamp(snapshot.source_seconds-r->source.source_seconds,1.0/120,.1);
    for(std::size_t i=0;i<snapshot.items.size();++i) {
      const auto& item=snapshot.items[i];
      r->starts[i]={{item.x,item.y,item.z,0},{item.block,item.count,0,0}};
      for(std::size_t j=0;j<r->displayed_count;++j)if(r->displayed_ids[j]==item.id) {
        r->starts[i]=r->displayed[j];break;
      }
    }
    r->source=snapshot;r->transition=0;
  }
  r->transition=std::min(r->transition+elapsed,r->duration);
  const float blend=static_cast<float>(r->transition/r->duration);
  Uniforms u{};r->current_view=view(camera,width,height);u.current=r->current_view;
  u.previous=!r->history||reset?u.current:r->committed_view;
  std::copy_n(lighting.light_direction,3,u.lighting);u.lighting[3]=lighting.sun_strength;
  u.settings[0]=lighting.ambient;u.settings[1]=static_cast<float>(r->animation);
  u.settings[2]=static_cast<float>(!r->history||reset?r->animation:r->committed_animation);
  u.settings[3]=std::clamp(mip_bias,-4.f,0.f);
  r->displayed_count=snapshot.items.size();
  for(std::size_t i=0;i<r->displayed_count;++i) {
    const auto& item=snapshot.items[i];auto& record=r->displayed[i];
    record={{std::lerp(r->starts[i].position[0],item.x,blend),std::lerp(r->starts[i].position[1],item.y,blend),
      std::lerp(r->starts[i].position[2],item.z,blend),static_cast<float>(item.id%997)},{item.block,item.count,0,0}};
    record.material[0]=static_cast<std::uint32_t>(std::min<std::uint64_t>(item.block,material_count-1));
    r->displayed_ids[i]=item.id;u.items[i]=record;u.previous_items[i]=record;
    u.items[i].material[3]=static_cast<unsigned>(i)*12;
    u.items[i].material[2]=1;
    if(r->history&&!reset)for(std::size_t j=0;j<r->committed_count;++j)if(r->committed_ids[j]==item.id) {
      u.previous_items[i]=r->committed[j];u.items[i].material[2]=0;break;
    }
  }
  r->prepared_provisional=r->provisional.has_value();
  if(r->provisional) {
    const auto& p=*r->provisional;
    r->provisional_record={{p.x,p.y,p.z,static_cast<float>(p.request%997)},
        {static_cast<std::uint32_t>(std::min<std::uint64_t>(p.block,material_count-1)),p.count,1,
        static_cast<unsigned>(r->displayed_count)*12}};
  }
  r->uniforms=u;
  if(world.gi_mode!=GiMode::BlockTransport)return prepare_dynamic_receivers(r->indirect,world,commands,{});
  const auto flags=world_atlas_flags(atlas);
  const std::size_t count=r->displayed_count+(r->prepared_provisional?1:0);
  std::vector<DynamicReceiver> receivers(count*12);
  for(std::size_t i=0;i<count;++i) {
    const bool provisional=i==r->displayed_count;
    const auto& record=provisional?r->provisional_record:r->displayed[i];
    if(record.material[0]>=flags.size())return false;
    const auto id=provisional?r->provisional->request:r->displayed_ids[i];
    const bool sprite=(flags[record.material[0]]&2)!=0;
    for(unsigned face=0;face<6;++face)for(unsigned side=0;side<2;++side) {
      auto& receiver=receivers[i*12+face*2+side];
      if(sprite && face!=0)continue;
      const auto geometry=item_receiver_geometry({record.position[0],record.position[1],record.position[2]},
          record.position[3],u.settings[1],face,sprite);
      const float sign=side?-1.f:1.f;
      receiver.position={geometry.position[0],geometry.position[1],geometry.position[2],0};
      receiver.normal={geometry.normal[0]*sign,geometry.normal[1]*sign,geometry.normal[2]*sign,
          std::bit_cast<float>((provisional?0x80000000u:0u)|(face*2+side))};
      receiver.identity={static_cast<std::uint32_t>(id),static_cast<std::uint32_t>(id>>32),0,0};
    }
  }
  return prepare_dynamic_receivers(r->indirect,world,commands,receivers);
}
bool render_world_items(WorldItemsRenderer* r,rhi::IRenderPassEncoder* pass,WorldAtlas* atlas,WorldRenderer& world,bool temporal) {
  if(!r || !pass || !atlas)return false;
  if(!r->displayed_count && !r->prepared_provisional)return true;
  auto u=r->uniforms;auto* materials=world_atlas_materials(atlas);if(!materials)return false;
  auto* root=pass->bindPipeline(temporal?r->temporal_pipeline:r->pipeline);
  if(!root)return false;
  const rhi::ShaderCursor cursor(root);
  if(SLANG_FAILED(cursor["blockMaterials"].setBinding(rhi::Binding(materials))) ||
      SLANG_FAILED(cursor["atlasAlbedo"].setBinding(rhi::Binding(world_atlas_albedo(atlas)))) ||
      SLANG_FAILED(cursor["atlasNearest"].setBinding(rhi::Binding(world_atlas_nearest(atlas)))) ||
      SLANG_FAILED(cursor["atlasFiltered"].setBinding(rhi::Binding(world_atlas_sprite(atlas)))) ||
      SLANG_FAILED(root->setData({0,0,0},&u,sizeof(u))) || !bind_world_atlas(atlas,root) ||
      !bind_dynamic_receivers(r->indirect,world,root))return false;
  rhi::DrawArguments draw{};draw.vertexCount=36;draw.instanceCount=static_cast<std::uint32_t>(r->displayed_count);
 if(draw.instanceCount)pass->draw(draw);
 if(r->prepared_provisional) {
  // Separate draw and identity domain: never enters authoritative interpolation/history.
  u.items[0]=r->provisional_record;
  u.previous_items[0]=u.items[0];
  if(SLANG_FAILED(root->setData({0,0,0},&u,sizeof(u))))return false;
  draw.instanceCount=1;pass->draw(draw);
 }
 return true;
}
void commit_world_items_frame(WorldItemsRenderer* r) {
  if(!r)return;r->committed=r->displayed;r->committed_ids=r->displayed_ids;
  r->committed_count=r->displayed_count;r->committed_view=r->current_view;
  r->committed_animation=r->animation;r->history=true;
}
}
