#include "WorldHdr.h"
#include "RhiShader.h"
namespace octaryn::client::rendering {
bool create_world_hdr(rhi::IDevice* device,WorldHdr& hdr) {
  return create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/Composite.slang","main",hdr.composite) &&
      create_rhi_compute_pipeline(device,"octaryn-client/Shaders/Hdr/Present.slang","main",hdr.present);
}
bool resize_world_hdr(rhi::IDevice* device,WorldHdr& hdr,unsigned width,unsigned height) {
  rhi::TextureDesc desc{};desc.size={width,height,1};
  desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::ShaderResource;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  for(unsigned i=0;i<4;++i) {
    hdr.views[i].setNull();hdr.gbuffer[i].setNull();desc.format=world_gbuffer_formats[i];
    if(SLANG_FAILED(device->createTexture(desc,nullptr,hdr.gbuffer[i].writeRef())) ||
       SLANG_FAILED(hdr.gbuffer[i]->getDefaultView(hdr.views[i].writeRef()))) return false;
  }
  hdr.scene_view.setNull();hdr.scene.setNull();desc.format=rhi::Format::RGBA16Float;
  desc.usage|=rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination;
  return SLANG_SUCCEEDED(device->createTexture(desc,nullptr,hdr.scene.writeRef())) &&
      SLANG_SUCCEEDED(hdr.scene->getDefaultView(hdr.scene_view.writeRef()));
}
bool composite_world_hdr(rhi::ICommandEncoder* commands,WorldHdr& hdr,float sky,float ambient,float twilight,float fog_distance,const float sun[4],unsigned width,unsigned height) {
  auto* pass=commands->beginComputePass();if(!pass) return false;
  auto* root=pass->bindPipeline(hdr.composite);bool ok=root!=nullptr;
  const float uniforms[12]={sky,ambient,twilight,fog_distance,sun[0],sun[1],sun[2],sun[3],float(width),float(height),0,0};
  if(ok) ok=SLANG_SUCCEEDED(root->setData({0,0,0},uniforms,sizeof(uniforms)));
  for(unsigned i=0;ok && i<4;++i) ok=SLANG_SUCCEEDED(root->setBinding({0,i,0},rhi::Binding(hdr.views[i])));
  if(ok) ok=SLANG_SUCCEEDED(root->setBinding({0,4,0},rhi::Binding(hdr.scene_view)));
  if(ok) pass->dispatchCompute((width+7)/8,(height+7)/8,1);
  pass->end();return ok;
}
bool present_world_hdr(rhi::ICommandEncoder* commands,WorldHdr& hdr,rhi::ITextureView* output,unsigned width,unsigned height,rhi::ITextureView* scene) {
  auto* pass=commands->beginComputePass();if(!pass) return false;
  auto* root=pass->bindPipeline(hdr.present);
  const unsigned options[4]={scene?1u:0u,0,0,0};
  bool ok=root && SLANG_SUCCEEDED(root->setBinding({0,0,0},rhi::Binding(scene?scene:hdr.scene_view.get()))) &&
      SLANG_SUCCEEDED(root->setData({0,0,0},options,sizeof(options))) &&
      SLANG_SUCCEEDED(root->setBinding({0,1,0},rhi::Binding(output)));
  if(ok) pass->dispatchCompute((width+7)/8,(height+7)/8,1);
  pass->end();return ok;
}
}
