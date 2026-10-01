#include "WorldHiz.h"
#include "RhiShader.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>
namespace octaryn::client::rendering {
bool create_world_hiz(rhi::IDevice* device,WorldHiz& hiz) {
  if(!create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Map/MapHiz.slang","hiz_main",hiz.pipeline))return false;
  rhi::SamplerDesc desc{};
  desc.addressU=desc.addressV=desc.addressW=rhi::TextureAddressingMode::ClampToEdge;
  desc.minFilter=desc.magFilter=desc.mipFilter=rhi::TextureFilteringMode::Point;
  desc.minLOD=0;desc.maxLOD=31;
  return SLANG_SUCCEEDED(device->createSampler(desc,hiz.sampler.writeRef()));
}
bool resize_world_hiz(rhi::IDevice* device,WorldHiz& hiz,unsigned width,unsigned height) {
  hiz.mip_views.clear();hiz.pyramid.setNull();hiz.width=width;hiz.height=height;hiz.valid=false;
  hiz.mips=1+unsigned(std::floor(std::log2(double(std::max(width,height)))));
  rhi::TextureDesc desc{};desc.size={width,height,1};desc.format=rhi::Format::R32Float;
  desc.mipCount=hiz.mips;
  desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::UnorderedAccess;
  desc.defaultState=rhi::ResourceState::ShaderResource;desc.label="world_hiz";
  if(SLANG_FAILED(device->createTexture(desc,nullptr,hiz.pyramid.writeRef())))return false;
  for(std::uint32_t mip=0;mip<hiz.mips;++mip) {
    rhi::TextureViewDesc view{};view.subresourceRange={0,1,mip,1};
    Slang::ComPtr<rhi::ITextureView> target;
    if(SLANG_FAILED(device->createTextureView(hiz.pyramid,view,target.writeRef())))return false;
    hiz.mip_views.push_back(std::move(target));
  }
  return true;
}
bool build_world_hiz(WorldHiz& hiz,rhi::ICommandEncoder* commands,rhi::ITexture* depth) {
  if(!hiz.pyramid||!hiz.pipeline||!depth)return false;
  commands->setTextureState(depth,rhi::ResourceState::ShaderResource);
  commands->setTextureState(hiz.pyramid,rhi::ResourceState::UnorderedAccess);
  auto* pass=commands->beginComputePass();if(!pass)return false;
  auto* root=pass->bindPipeline(hiz.pipeline);bool okay=root!=nullptr;
  unsigned width=hiz.width,height=hiz.height;
  for(std::uint32_t mip=0;okay && mip<hiz.mips;++mip) {
    rhi::ShaderCursor cursor(root);
    okay=SLANG_SUCCEEDED(cursor["hizSource"].setBinding(mip?rhi::Binding(hiz.mip_views[mip-1]):rhi::Binding(depth))) &&
        SLANG_SUCCEEDED(cursor["hizTarget"].setBinding(rhi::Binding(hiz.mip_views[mip])));
    if(okay)pass->dispatchCompute((width+7)/8,(height+7)/8,1);
    width=std::max(1u,width/2);height=std::max(1u,height/2);
  }
  pass->end();
  commands->setTextureState(hiz.pyramid,rhi::ResourceState::ShaderResource);
  hiz.valid=hiz.valid||okay;
  return okay;
}
}
