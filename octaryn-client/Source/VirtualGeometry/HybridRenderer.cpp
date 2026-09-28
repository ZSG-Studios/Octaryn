#include "HybridRenderer.h"
#include "../Rendering/RenderBackend/RhiShader.h"
#include <slang-rhi/shader-cursor.h>
#include <filesystem>
#include <vector>
#include <limits>
#include <cstdio>
namespace octaryn::client::rendering::virtual_geometry {
namespace {
bool buffer_binding(rhi::ShaderCursor& cursor,const char* name,rhi::IBuffer* buffer) {
  auto field=cursor[name];return !field.isValid() || (buffer && SLANG_SUCCEEDED(field.setBinding(rhi::Binding(buffer))));
}
bool uniforms(rhi::ShaderCursor& cursor,const char* name,const void* data,size_t bytes) {
  auto field=cursor[name];return !field.isValid() || SLANG_SUCCEEDED(field.setData(data,bytes));
}
}
bool HybridRenderer::initialize(rhi::IDevice* device,const char* directory,std::span<const rhi::Format> targets,rhi::Format depth) {
  const auto path=[&](const char* name){return (std::filesystem::path(directory)/name).generic_string();};
  if(!create_rhi_compute_pipeline(device,path("Visibility.slang").c_str(),"clear_main",clear_) ||
      !create_rhi_compute_pipeline(device,path("HybridRaster.slang").c_str(),"software_main",software_))return false;
  Slang::ComPtr<rhi::IShaderProgram> program;
  const char* raster[]{"amplification_main","mesh_main","fragment_main"};
  if(!create_rhi_program(device,path("HybridRaster.slang").c_str(),raster,3,program))return false;
  rhi::RenderPipelineDesc desc{};desc.program=program;desc.rasterizer.cullMode=rhi::CullMode::None;
  desc.rasterizer.frontFace=rhi::FrontFaceMode::CounterClockwise;
  if(SLANG_FAILED(device->createRenderPipeline(desc,hardware_.writeRef()))) {
    std::fputs("geometry_pipeline_failed stage=hardware\n",stderr);return false;
  }
  const char* material[]{"vertex_main","fragment_main"};
  if(!create_rhi_program(device,path("MaterialResolve.slang").c_str(),material,2,program))return false;
  std::vector<rhi::ColorTargetDesc> colors(targets.size());
  for(size_t i=0;i<colors.size();++i)colors[i].format=targets[i];
  desc.program=program;desc.targetCount=static_cast<uint32_t>(colors.size());desc.targets=colors.data();
  desc.depthStencil.format=depth;desc.depthStencil.depthTestEnable=true;
  desc.depthStencil.depthWriteEnable=true;desc.depthStencil.depthFunc=rhi::ComparisonFunc::LessEqual;
  if(SLANG_FAILED(device->createRenderPipeline(desc,resolve_.writeRef()))) {
    std::fputs("geometry_pipeline_failed stage=resolve\n",stderr);return false;
  }
  return true;
}
bool HybridRenderer::resize(rhi::IDevice* device,std::uint32_t width,std::uint32_t height) {
  if(!width || !height || std::uint64_t(width)*height>std::numeric_limits<std::uint32_t>::max())return false;
  if(visibility_ && width_==width && height_==height)return true;
  rhi::BufferDesc desc{};desc.size=std::uint64_t(width)*height*8;desc.elementSize=8;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;desc.label="geometry_visibility";
  Slang::ComPtr<rhi::IBuffer> next;
  if(SLANG_FAILED(device->createBuffer(desc,nullptr,next.writeRef())))return false;
  visibility_=next;width_=width;height_=height;return true;
}
bool HybridRenderer::bind(rhi::IShaderObject* root,const HybridInputs& input,bool resolve) {
  if(!root)return false;rhi::ShaderCursor cursor(root);
  const uint32_t extent[]{input.width,input.height,input.selected_capacity,input.pool_slots};
  return buffer_binding(cursor,"geometryClusters",input.clusters) && buffer_binding(cursor,"geometrySelected",input.selected) &&
      buffer_binding(cursor,"geometryCounters",input.counters) && buffer_binding(cursor,"geometryPageTable",input.page_table) &&
      buffer_binding(cursor,"geometryPool",input.pool) && buffer_binding(cursor,"geometryMaterials",input.materials) &&
      buffer_binding(cursor,"geometryOcclusionFlags",input.occlusion_flags?input.occlusion_flags:input.counters) &&
      uniforms(cursor,"geometryOcclusionPhase",&input.occlusion_phase,sizeof(input.occlusion_phase)) &&
      buffer_binding(cursor,resolve?"geometryVisibilityRead":"geometryVisibility",visibility_) &&
      uniforms(cursor,"geometryView",input.view.data(),sizeof(input.view)) &&
      uniforms(cursor,"geometryExtent",extent,sizeof(extent)) && uniforms(cursor,"geometryAmbient",input.ambient.data(),sizeof(input.ambient));
}
bool HybridRenderer::visibility(rhi::ICommandEncoder* commands,const HybridInputs& input,bool clear_visibility) {
  if(!commands || !visibility_ || width_!=input.width || height_!=input.height || !input.dispatch ||
      input.selected_capacity>(UINT32_MAX/2-1)/128)return false;
  if(input.occlusion_phase && !input.occlusion_flags)return false;
  if(clear_visibility) {
    auto* clear=commands->beginComputePass();
    if(!bind(clear->bindPipeline(clear_),input,false)){clear->end();return false;}
    clear->dispatchCompute((width_*height_+255)/256,1,1);clear->end();
  }
  auto* software=commands->beginComputePass();
  if(!bind(software->bindPipeline(software_),input,false)){software->end();return false;}
  software->dispatchComputeIndirect({input.dispatch,12});software->end();
  rhi::RenderPassDesc passDesc{};auto* hardware=commands->beginRenderPass(passDesc);
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(width_),float(height_));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(width_,height_);hardware->setRenderState(state);
  if(!bind(hardware->bindPipeline(hardware_),input,false)){hardware->end();return false;}
  hardware->drawMeshTasksIndirect(1,{input.dispatch,0});hardware->end();return true;
}
bool HybridRenderer::resolve(rhi::IRenderPassEncoder* pass,const HybridInputs& input) {
  if(!pass || !visibility_ || !bind(pass->bindPipeline(resolve_),input,true))return false;
  rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
  state.viewports[0]=rhi::Viewport::fromSize(float(width_),float(height_));
  state.scissorRects[0]=rhi::ScissorRect::fromSize(width_,height_);pass->setRenderState(state);
  rhi::DrawArguments draw{};draw.vertexCount=3;pass->draw(draw);return true;
}
}
