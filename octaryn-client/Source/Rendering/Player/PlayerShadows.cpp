#include "PlayerRendererInternal.h"
#include "RhiShader.h"
#include <slang-rhi/acceleration-structure-utils.h>
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering {
namespace {
bool buffer(PlayerRenderer& r,std::uint64_t size,unsigned stride,rhi::BufferUsage usage,
    rhi::ResourceState state,Slang::ComPtr<rhi::IBuffer>& target) {
  if(target)return true;
  rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;desc.usage=usage;desc.defaultState=state;
  return SLANG_SUCCEEDED(r.device->createBuffer(desc,nullptr,target.writeRef()));
}
bool build(PlayerRenderer& r,rhi::ICommandEncoder* commands,rhi::AccelerationStructureBuildInput& input,
    rhi::AccelerationStructureKind kind,Slang::ComPtr<rhi::IAccelerationStructure>& target,
    Slang::ComPtr<rhi::IBuffer>& scratch) {
  rhi::AccelerationStructureBuildDesc desc{};desc.inputs=&input;desc.inputCount=1;
  desc.flags=rhi::AccelerationStructureBuildFlags::PreferFastBuild;
  if(!target) {
    rhi::AccelerationStructureSizes sizes{};
    if(SLANG_FAILED(r.device->getAccelerationStructureSizes(desc,&sizes)) || !sizes.accelerationStructureSize)return false;
    rhi::AccelerationStructureDesc allocation{};allocation.kind=kind;allocation.size=sizes.accelerationStructureSize;
    allocation.label="animated_player_shadow";
    if(SLANG_FAILED(r.device->createAccelerationStructure(allocation,target.writeRef())) ||
       !buffer(r,sizes.scratchSize,4,rhi::BufferUsage::UnorderedAccess,rhi::ResourceState::UnorderedAccess,scratch))return false;
  }
  // The frame fence protects these retained destinations; rebuilding avoids refit degradation.
  commands->globalBarrier();
  commands->buildAccelerationStructure(desc,target,nullptr,scratch,0,nullptr);
  commands->globalBarrier();return true;
}
bool build_ray_scene(PlayerRenderer& r,rhi::ICommandEncoder* commands) {
  auto& frame=r.shadows[r.shadow_slot];
  rhi::AccelerationStructureBuildInput triangles{};triangles.type=rhi::AccelerationStructureBuildInputType::Triangles;
  auto& mesh=triangles.triangles;mesh.vertexBuffers[0]=frame.positions;mesh.vertexBufferCount=1;
  mesh.vertexFormat=rhi::Format::RGB32Float;mesh.vertexCount=static_cast<unsigned>(r.model.vertices.size());mesh.vertexStride=16;
  mesh.indexBuffer=r.indices;mesh.indexFormat=rhi::IndexFormat::Uint32;
  mesh.indexCount=static_cast<unsigned>(r.model.indices.size());mesh.flags=rhi::AccelerationStructureGeometryFlags::Opaque;
  if(!build(r,commands,triangles,rhi::AccelerationStructureKind::BottomLevel,frame.blas,frame.blas_scratch))return false;
  rhi::AccelerationStructureInstanceDescGeneric instance{};
  instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
  instance.instanceMask=255;instance.accelerationStructure=frame.blas->getHandle();
  const auto type=rhi::getAccelerationStructureInstanceDescType(r.device);
  const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
  std::vector<std::uint8_t> native(stride);
  rhi::convertAccelerationStructureInstanceDescs(1,type,native.data(),stride,&instance,sizeof(instance));
  if(!buffer(r,stride,static_cast<unsigned>(stride),rhi::BufferUsage::AccelerationStructureBuildInput|rhi::BufferUsage::CopyDestination,
      rhi::ResourceState::AccelerationStructureBuildInput,frame.instances) ||
      SLANG_FAILED(commands->uploadBufferData(frame.instances,0,stride,native.data())))return false;
  rhi::AccelerationStructureBuildInput instances{};instances.type=rhi::AccelerationStructureBuildInputType::Instances;
  instances.instances.instanceBuffer=frame.instances;instances.instances.instanceStride=static_cast<unsigned>(stride);
  instances.instances.instanceCount=1;
  return build(r,commands,instances,rhi::AccelerationStructureKind::TopLevel,frame.tlas,frame.tlas_scratch);
}
}
bool sample_player_frame(PlayerRenderer& r,const PlayerPose& pose) {
  const auto& old=r.skin_pose;
  if(r.skin_valid && old.source_seconds==pose.source_seconds && old.clip==pose.clip && old.action_sequence==pose.action_sequence)return true;
  constexpr const char* clips[]={"idle_loop","walk_loop","run_loop","crouch_walk_loop",
      "jump_once","fall_loop","attack_slash_once","wave_loop"};
  const auto clip=static_cast<size_t>(pose.clip);r.skin_valid=false;
  if(clip>=std::size(clips) || !std::isfinite(pose.source_seconds) ||
      !r.animation.sample(r.model,clips[clip],pose.action_sequence,pose.source_seconds,r.skin))return false;
  r.skin_pose=pose;r.skin_valid=true;return true;
}
bool prepare_player_shadows(PlayerRenderer* renderer,rhi::ICommandEncoder* commands,unsigned slot,const PlayerPose& pose,bool ray_tracing) {
  if(!renderer)return true;
  auto& r=*renderer;r.shadow_visible=false;
  if(!commands || slot>=r.shadows.size())return false;
  r.shadow_slot=slot;
  if(!pose.visible)return true;
  if(!sample_player_frame(r,pose))return false;
  if(!r.skin_pipeline && !create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Player/PlayerShadow.slang","skin_main",r.skin_pipeline))return false;
  auto& frame=r.shadows[slot];
  const unsigned count=static_cast<unsigned>(r.model.vertices.size());
  auto usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::ShaderResource;
  if(r.device->hasFeature(rhi::Feature::AccelerationStructure))usage|=rhi::BufferUsage::AccelerationStructureBuildInput;
  if(!buffer(r,std::uint64_t(count)*16,16,usage,rhi::ResourceState::ShaderResource,frame.positions))return false;
  float joints[PlayerMaxJoints*4][4];
  for(size_t joint=0;joint<r.skin.size();++joint)for(size_t column=0;column<4;++column)for(size_t row=0;row<4;++row)
    joints[joint*4+column][row]=r.skin[joint][column][row];
  const float feet[4]={pose.feet_x,pose.feet_y,pose.feet_z,0},rotation[4]={std::cos(pose.yaw),std::sin(pose.yaw),0,0};
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(r.skin_pipeline);bool ok=root!=nullptr;
  if(ok) {
    rhi::ShaderCursor c(root);
    ok=SLANG_SUCCEEDED(c["vertices"].setBinding(r.vertices)) && SLANG_SUCCEEDED(c["skinnedOutput"].setBinding(frame.positions)) &&
      SLANG_SUCCEEDED(c["jointColumns"].setData(joints,sizeof(joints))) && SLANG_SUCCEEDED(c["vertexCount"].setData(&count,sizeof(count))) &&
      SLANG_SUCCEEDED(c["feetPosition"].setData(feet,sizeof(feet))) && SLANG_SUCCEEDED(c["bodyRotation"].setData(rotation,sizeof(rotation)));
  }
  if(ok)pass->dispatchCompute((count+63)/64,1,1);pass->end();if(!ok)return false;
  if(ray_tracing && !build_ray_scene(r,commands))return false;
  commands->setBufferState(frame.positions,rhi::ResourceState::ShaderResource);
  r.shadow_visible=true;return true;
}
bool bind_player_shadows(PlayerRenderer* renderer,rhi::IShaderObject* root,rhi::IAccelerationStructure* empty_scene) {
  rhi::ShaderCursor c(root);auto scene=c["playerShadowScene"],enabled=c["playerShadowEnabled"];
  // Some world-ray programs only inspect terrain and optimize shadow visibility away.
  if(!scene.isValid() && !enabled.isValid())return true;
  auto* tlas=renderer?renderer->shadows[renderer->shadow_slot].tlas.get():nullptr;
  const unsigned active=renderer && renderer->shadow_visible && tlas?1u:0u;
  return (!scene.isValid() || SLANG_SUCCEEDED(scene.setBinding(rhi::Binding(tlas?tlas:empty_scene)))) &&
    (!enabled.isValid() || SLANG_SUCCEEDED(enabled.setData(&active,sizeof(active))));
}
bool render_player_shadow(PlayerRenderer* renderer,rhi::IRenderPassEncoder* pass,const float* center,
    const float* right,const float* up,const float* forward,const float* projection) {
  if(!renderer || !renderer->shadow_visible)return true;
  auto& r=*renderer;
  if(!r.shadow_pipeline) {
    const char* entries[]={"vertex_main","fragment_main"};Slang::ComPtr<rhi::IShaderProgram> program;
    if(!create_rhi_program(r.device,"octaryn-client/Shaders/Player/PlayerShadow.slang",entries,2,program))return false;
    rhi::RenderPipelineDesc desc{};desc.program=program;desc.rasterizer.cullMode=rhi::CullMode::None;
    desc.depthStencil.format=rhi::Format::D32Float;desc.depthStencil.depthTestEnable=true;desc.depthStencil.depthWriteEnable=true;
    desc.depthStencil.depthFunc=rhi::ComparisonFunc::Less;
    if(SLANG_FAILED(r.device->createRenderPipeline(desc,r.shadow_pipeline.writeRef())))return false;
  }
  auto* root=pass->bindPipeline(r.shadow_pipeline);if(!root)return false;
  rhi::ShaderCursor c(root);
  if(SLANG_FAILED(c["indices"].setBinding(r.indices)) || SLANG_FAILED(c["skinnedPositions"].setBinding(r.shadows[r.shadow_slot].positions)) ||
      SLANG_FAILED(c["shadowCenter"].setData(center,16)) || SLANG_FAILED(c["shadowRight"].setData(right,16)) ||
      SLANG_FAILED(c["shadowUp"].setData(up,16)) || SLANG_FAILED(c["shadowForward"].setData(forward,16)) ||
      SLANG_FAILED(c["shadowProjection"].setData(projection,16)))return false;
  for(const auto& primitive:r.model.primitives) {
    if(!primitive.count || primitive.color[3]<primitive.alpha_cutoff)continue;
    if(SLANG_FAILED(c["firstIndex"].setData(&primitive.first,sizeof(primitive.first))))return false;
    rhi::DrawArguments draw{};draw.vertexCount=primitive.count;pass->draw(draw);
  }
  return true;
}
}
