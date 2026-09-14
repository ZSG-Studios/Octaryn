#include "PlayerRendererInternal.h"
#include "RhiShader.h"
#include "WorldRenderer.h"
#include "TemporalCamera.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

namespace octaryn::client::rendering {
PlayerRenderer* create_player_renderer(rhi::IDevice* device,rhi::Format color_format,
    rhi::Format depth_format,const char* asset_path,const char* shader_path) {
  if(!device || !asset_path || !shader_path) return nullptr;
  auto renderer=std::make_unique<PlayerRenderer>();renderer->device=device;
  std::string error;
  if(!load_player_model(std::filesystem::path(reinterpret_cast<const char8_t*>(asset_path)),renderer->model,error)) {
    std::fprintf(stderr,"player_model_load_failed: %s\n",error.c_str());return nullptr;
  }
  rhi::BufferDesc buffer{};buffer.usage=rhi::BufferUsage::ShaderResource;
  buffer.defaultState=rhi::ResourceState::ShaderResource;
  buffer.size=renderer->model.vertices.size()*sizeof(PlayerVertex);buffer.elementSize=sizeof(PlayerVertex);
  if(SLANG_FAILED(device->createBuffer(buffer,renderer->model.vertices.data(),renderer->vertices.writeRef()))) return nullptr;
  auto indices=renderer->model.indices;
  indices.insert(indices.end(),renderer->model.first_person_indices.begin(),renderer->model.first_person_indices.end());
  buffer.size=indices.size()*sizeof(uint32_t);buffer.elementSize=sizeof(uint32_t);
  if(device->hasFeature(rhi::Feature::AccelerationStructure))buffer.usage|=rhi::BufferUsage::AccelerationStructureBuildInput;
  if(SLANG_FAILED(device->createBuffer(buffer,indices.data(),renderer->indices.writeRef()))) return nullptr;
  const char* entries[]={"vertex_main","fragment_main"};
  Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(device,shader_path,entries,2,program)) return nullptr;
  rhi::ColorTargetDesc target{};target.format=color_format;
  rhi::RenderPipelineDesc desc{};desc.program=program;desc.targets=&target;desc.targetCount=1;
  desc.rasterizer.cullMode=rhi::CullMode::None;
  desc.depthStencil.format=depth_format;desc.depthStencil.depthTestEnable=true;
  desc.depthStencil.depthWriteEnable=true;desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  if(SLANG_FAILED(device->createRenderPipeline(desc,renderer->pipeline.writeRef()))) return nullptr;
  const char* temporal_entries[]={"vertex_main","temporal_fragment_main"};
  if(!create_rhi_program(device,shader_path,temporal_entries,2,program))return nullptr;
  rhi::ColorTargetDesc temporal_targets[2]{};
  temporal_targets[0].format=color_format;temporal_targets[1].format=rhi::Format::RGBA16Float;
  desc.program=program;desc.targets=temporal_targets;desc.targetCount=2;
  if(SLANG_FAILED(device->createRenderPipeline(desc,renderer->temporal_pipeline.writeRef())))return nullptr;
  std::printf("player_model_loaded vertices=%zu indices=%zu joints=%zu clips=%zu primitives=%zu source=octaryn_player_v1.gltf\n",
    renderer->model.vertices.size(),renderer->model.indices.size(),renderer->model.joints.size(),
    renderer->model.animations.size(),renderer->model.primitives.size());
  return renderer.release();
}
void destroy_player_renderer(PlayerRenderer* renderer) {delete renderer;}
bool render_player(PlayerRenderer* renderer,rhi::IRenderPassEncoder* pass,const WorldCamera& camera,
    int width,int height,const PlayerPose& pose,const PlayerLighting& lighting,bool temporal,bool reset) {
  if(!renderer || !pass || width<=0 || height<=0) return false;
  renderer->pending_valid=false;
  if(!pose.visible) return true;
  if(!sample_player_frame(*renderer,pose))return false;
  const auto& skin=renderer->skin;
  struct Uniforms {
    float joints[PlayerMaxJoints*4][4];
    float camera[4],right[4],up[4],forward[4],projection[4],feet[4],rotation[4];
    float light[4],settings[4],color[4],material[4];
    float previous_joints[PlayerMaxJoints*4][4];
    TemporalView previous_view;
    float previous_feet[4],previous_rotation[4];
  } uniforms{};
  static_assert(sizeof(fastgltf::math::fmat4x4)==64);
  static_assert(sizeof(Uniforms)==8192+18*16);
  const bool use_previous=renderer->previous_valid && !reset && renderer->previous_pose.first_person==pose.first_person;
  const auto& previous_skin=use_previous?renderer->previous_skin:skin;
  for(size_t joint=0;joint<skin.size();++joint) for(size_t column=0;column<4;++column)
    for(size_t row=0;row<4;++row) {
      uniforms.joints[joint*4+column][row]=skin[joint][column][row];
      uniforms.previous_joints[joint*4+column][row]=previous_skin[joint][column][row];
    }
  uniforms.previous_view=temporal_view(use_previous?renderer->previous_camera:camera,
      use_previous?renderer->previous_width:width,use_previous?renderer->previous_height:height);
  const auto& previous_pose=use_previous?renderer->previous_pose:pose;
  uniforms.previous_feet[0]=previous_pose.feet_x;uniforms.previous_feet[1]=previous_pose.feet_y;uniforms.previous_feet[2]=previous_pose.feet_z;
  uniforms.previous_rotation[0]=std::cos(previous_pose.yaw);uniforms.previous_rotation[1]=std::sin(previous_pose.yaw);
  const float sy=std::sin(camera.yaw),cy=std::cos(camera.yaw),sp=std::sin(camera.pitch),cp=std::cos(camera.pitch);
  const float focal=1/std::tan(std::clamp(camera.vertical_fov,.2f,2.7f)/2);
  constexpr float near_plane=.1f,far_plane=8192;
  const float view[5][4]={{camera.x,camera.y,camera.z,0},{cy,0,sy,camera.jitter_x},{-sy*sp,cp,cy*sp,camera.jitter_y},
    {sy*cp,sp,-cy*cp,0},{focal*static_cast<float>(height)/static_cast<float>(width),focal,
    far_plane/(far_plane-near_plane),near_plane*far_plane/(far_plane-near_plane)}};
  std::memcpy(uniforms.camera,view,sizeof(view));
  uniforms.feet[0]=pose.feet_x;uniforms.feet[1]=pose.feet_y;uniforms.feet[2]=pose.feet_z;
  uniforms.rotation[0]=std::cos(pose.yaw);uniforms.rotation[1]=std::sin(pose.yaw);
  std::copy_n(lighting.light_direction,3,uniforms.light);
  uniforms.settings[0]=lighting.ambient;uniforms.settings[1]=lighting.sun_strength;
  for(const auto& primitive:renderer->model.primitives) {
    const auto count=pose.first_person?primitive.first_person_count:primitive.count;
    if(!count) continue;
    std::copy(primitive.color.begin(),primitive.color.end(),uniforms.color);
    uniforms.material[0]=primitive.metallic;uniforms.material[1]=primitive.roughness;
    uniforms.material[2]=primitive.alpha_cutoff;
    uniforms.material[3]=static_cast<float>(pose.first_person?renderer->model.indices.size()+primitive.first_person_first:primitive.first);
    auto* root=pass->bindPipeline(temporal?renderer->temporal_pipeline:renderer->pipeline);
    if(!root || SLANG_FAILED(root->setBinding({0,0,0},rhi::Binding(renderer->vertices))) ||
        SLANG_FAILED(root->setBinding({0,1,0},rhi::Binding(renderer->indices))) ||
        SLANG_FAILED(root->setData({0,0,0},&uniforms,sizeof(uniforms)))) return false;
    rhi::DrawArguments draw{};draw.vertexCount=count;pass->draw(draw);
  }
  renderer->pending_width=width;renderer->pending_height=height;
  renderer->pending_skin=skin;renderer->pending_pose=pose;renderer->pending_camera=camera;renderer->pending_valid=true;
  return true;
}
void commit_player_frame(PlayerRenderer* renderer) {
  if(!renderer)return;
  renderer->previous_valid=renderer->pending_valid;
  if(!renderer->pending_valid)return;
  renderer->previous_skin=renderer->pending_skin;renderer->previous_pose=renderer->pending_pose;
  renderer->previous_camera=renderer->pending_camera;
  renderer->previous_width=renderer->pending_width;renderer->previous_height=renderer->pending_height;
}
}
