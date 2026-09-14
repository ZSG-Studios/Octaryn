#include "SelectionRenderer.h"
#include "WorldRenderer.h"
#include "RhiShader.h"
#include <algorithm>
#include <cmath>
namespace octaryn::client::rendering {
bool create_selection_pipeline(rhi::IDevice* device,rhi::Format color,rhi::Format depth,
    Slang::ComPtr<rhi::IRenderPipeline>& pipeline) {
  const char* entries[]={"vertex_main","fragment_main"};
  Slang::ComPtr<rhi::IShaderProgram> program;
  if(!create_rhi_program(device,"octaryn-client/Shaders/Selection/Selection.slang",entries,2,program)) return false;
  rhi::ColorTargetDesc target{};target.format=color;target.enableBlend=true;
  target.color.srcFactor=rhi::BlendFactor::SrcAlpha;target.color.dstFactor=rhi::BlendFactor::InvSrcAlpha;
  target.alpha.srcFactor=rhi::BlendFactor::One;target.alpha.dstFactor=rhi::BlendFactor::InvSrcAlpha;
  rhi::RenderPipelineDesc desc{};desc.program=program;desc.targets=&target;desc.targetCount=1;
  desc.depthStencil.format=depth;desc.depthStencil.depthTestEnable=true;
  desc.depthStencil.depthWriteEnable=false;desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  return SLANG_SUCCEEDED(device->createRenderPipeline(desc,pipeline.writeRef()));
}
bool render_selection(rhi::IRenderPassEncoder* pass,rhi::IRenderPipeline* pipeline,const WorldCamera& eye,
                       int width,int height,const SelectionTarget& target) {
  if(target.face>=6) return true;
  if(!pass || !pipeline || width<=0 || height<=0) return false;
  const float sy=std::sin(eye.yaw),cy=std::cos(eye.yaw),sp=std::sin(eye.pitch),cp=std::cos(eye.pitch);
  const float focal=1/std::tan(std::clamp(eye.vertical_fov,.2f,2.7f)/2);
  const float data[]={eye.x,eye.y,eye.z,0,cy,0,sy,eye.jitter_x,-sy*sp,cp,cy*sp,eye.jitter_y,sy*cp,sp,-cy*cp,0,
    focal*static_cast<float>(height)/static_cast<float>(width),focal,8192/8191.9f,819.2f/8191.9f,
    static_cast<float>(target.x),static_cast<float>(target.y),static_cast<float>(target.z),static_cast<float>(target.face)};
  auto* root=pass->bindPipeline(pipeline);
  if(!root || SLANG_FAILED(root->setData({0,0,0},data,sizeof(data)))) return false;
  rhi::DrawArguments draw{};draw.vertexCount=6;pass->draw(draw);return true;
}
}
