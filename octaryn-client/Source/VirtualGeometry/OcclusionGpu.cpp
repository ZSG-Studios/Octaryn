#include "OcclusionGpu.h"
#include "../Rendering/RenderBackend/RhiShader.h"
#include <slang-com-ptr.h>
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <vector>

namespace octaryn::client::rendering::virtual_geometry {
using Slang::ComPtr;
struct OcclusionGpu::State {
  struct Frame {
    ComPtr<rhi::IBuffer> flags,counters;
    ComPtr<rhi::ITexture> pyramid;
    std::vector<ComPtr<rhi::ITextureView>> views;
    std::array<float,20> view{};
    unsigned width{},height{},padded_width{},padded_height{};
    bool valid{};
  };
  ComPtr<rhi::IDevice> device;
  std::array<ComPtr<rhi::IComputePipeline>,5> pipelines;
  std::array<Frame,2> frames;
  OcclusionInputs input;
  unsigned capacity{},slot{},previous{2};
  bool pending{},built{};
  std::string error;
  bool fail(const char* value) {error=value;return false;}
  bool resize(Frame& frame,unsigned width,unsigned height) {
    if(frame.pyramid && frame.width==width && frame.height==height)return true;
    frame.width=width;frame.height=height;frame.valid=false;
    frame.padded_width=std::bit_ceil(width);frame.padded_height=std::bit_ceil(height);
    rhi::TextureDesc desc{};desc.size={frame.padded_width,frame.padded_height,1};desc.format=rhi::Format::R32Float;
    desc.mipCount=std::bit_width(std::max(frame.padded_width,frame.padded_height));
    desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::UnorderedAccess;
    desc.defaultState=rhi::ResourceState::ShaderResource;desc.label="geometry_occlusion_pyramid";
    if(SLANG_FAILED(device->createTexture(desc,nullptr,frame.pyramid.writeRef())))return fail("occlusion pyramid allocation failed");
    frame.views.clear();
    for(unsigned mip=0;mip<desc.mipCount;++mip) {
      rhi::TextureViewDesc view{};view.subresourceRange={0,1,mip,1};
      ComPtr<rhi::ITextureView> target;
      if(SLANG_FAILED(device->createTextureView(frame.pyramid,view,target.writeRef())))return fail("occlusion mip allocation failed");
      frame.views.push_back(target);
    }
    return true;
  }
  bool compatible(const Frame& history) const {
    if(!history.valid || history.width!=input.width || history.height!=input.height)return false;
    float displacement=0;
    for(unsigned i=0;i<3;++i) {const auto delta=input.view[i]-history.view[i];displacement+=delta*delta;}
    if(displacement>64)return false;
    for(unsigned axis:{4u,8u,12u}) {
      float dot=0;for(unsigned i=0;i<3;++i)dot+=input.view[axis+i]*history.view[axis+i];
      if(dot<.98f)return false;
    }
    for(unsigned i=16;i<20;++i)if(std::abs(input.view[i]-history.view[i])>1e-4f)return false;
    return true;
  }
  bool classify(rhi::ICommandEncoder* commands,unsigned pipeline,const Frame& pyramid,
      const std::array<float,20>& view,bool history) {
    auto& frame=frames[slot];auto* pass=commands->beginComputePass();if(!pass)return fail("occlusion compute pass failed");
    auto* root=pass->bindPipeline(pipelines[pipeline]);bool ok=root!=nullptr;
    if(root) {
      rhi::ShaderCursor c(root);
      const auto bind=[&](const char* name,rhi::IBuffer* value){return SLANG_SUCCEEDED(c[name].setBinding(rhi::Binding(value)));};
      ok=bind("occlusionCounters",frame.counters);
      if(pipeline!=0) {
        const unsigned settings[]{capacity,input.width,input.height,history?1u:0u};
        const unsigned size[]{pyramid.padded_width,pyramid.padded_height,unsigned(pyramid.views.size()),0};
        ok=ok&&bind("occlusionClusters",input.clusters)&&bind("occlusionSelected",input.selected)&&
            bind("occlusionSelectionCounters",input.selection_counters)&&bind("occlusionFlags",frame.flags)&&
            SLANG_SUCCEEDED(c["occlusionPyramid"].setBinding(rhi::Binding(pyramid.pyramid)))&&
            SLANG_SUCCEEDED(c["occlusionView"].setData(view.data(),sizeof(view)))&&
            SLANG_SUCCEEDED(c["occlusionSettings"].setData(settings,sizeof(settings)))&&
            SLANG_SUCCEEDED(c["occlusionPyramidSize"].setData(size,sizeof(size)));
      }
    }
    const auto groups=pipeline==0?1u:capacity/64+unsigned(capacity%64!=0);
    if(ok)pass->dispatchCompute(std::min(groups,65535u),std::max(1u,groups/65535+unsigned(groups%65535!=0)),1);
    pass->end();commands->globalBarrier();return ok || fail("occlusion classification binding failed");
  }
};
OcclusionGpu::OcclusionGpu():state_(std::make_unique<State>()) {}
OcclusionGpu::~OcclusionGpu()=default;
const std::string& OcclusionGpu::error() const {return state_->error;}
rhi::IBuffer* OcclusionGpu::flags() const {return state_->frames[state_->slot].flags;}
rhi::IBuffer* OcclusionGpu::counters() const {return state_->frames[state_->slot].counters;}
std::uint64_t OcclusionGpu::gpu_bytes() const {
  std::uint64_t bytes=0;
  for(const auto& f:state_->frames) {
    if(f.flags)bytes+=f.flags->getDesc().size;if(f.counters)bytes+=f.counters->getDesc().size;
    for(unsigned mip=0;mip<f.views.size();++mip)
      bytes+=std::uint64_t(std::max(1u,f.padded_width>>mip))*std::max(1u,f.padded_height>>mip)*4;
  }
  return bytes;
}
bool OcclusionGpu::initialize(rhi::IDevice* device,const char* path,std::uint32_t capacity) {
  auto& s=*state_;if(s.device || !device || !path || !capacity)return s.fail("invalid occlusion configuration");
  s.device=device;s.capacity=capacity;
  const char* entries[]{"reset_main","classify_main","retest_main","depth_main","reduce_main"};
  for(unsigned i=0;i<s.pipelines.size();++i)
    if(!create_rhi_compute_pipeline(device,path,entries[i],s.pipelines[i]))return s.fail("occlusion pipeline creation failed");
  for(auto& frame:s.frames) {
    rhi::BufferDesc desc{};desc.size=std::uint64_t(capacity)*4;desc.elementSize=4;
    desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
    desc.defaultState=rhi::ResourceState::ShaderResource;desc.label="geometry_occlusion_flags";
    if(SLANG_FAILED(device->createBuffer(desc,nullptr,frame.flags.writeRef())))return s.fail("occlusion flag allocation failed");
    desc.size=16;desc.label="geometry_occlusion_counters";
    if(SLANG_FAILED(device->createBuffer(desc,nullptr,frame.counters.writeRef())))return s.fail("occlusion counter allocation failed");
  }
  return true;
}
bool OcclusionGpu::begin(rhi::ICommandEncoder* commands,std::uint32_t slot,const OcclusionInputs& input) {
  auto& s=*state_;
  if(!s.device || !commands || s.pending || slot>=2 || !input.width || !input.height ||
      input.width>16384 || input.height>16384 || !input.clusters || !input.selected || !input.selection_counters)
    return s.fail("invalid occlusion begin");
  for(float value:input.view)if(!std::isfinite(value))return s.fail("nonfinite occlusion view");
  s.slot=slot;s.input=input;s.built=false;
  auto& frame=s.frames[slot];if(!s.resize(frame,input.width,input.height))return false;
  const bool valid=s.previous<2 && s.compatible(s.frames[s.previous]);
  const auto& history=valid?s.frames[s.previous]:frame;
  if(!s.classify(commands,0,frame,input.view,false)||!s.classify(commands,1,history,valid?history.view:input.view,valid))return false;
  s.pending=true;return true;
}
bool OcclusionGpu::build_current(rhi::ICommandEncoder* commands,rhi::IBuffer* visibility) {
  auto& s=*state_;if(!s.pending || !commands || !visibility)return s.fail("invalid occlusion build");
  auto& frame=s.frames[s.slot];unsigned width=frame.padded_width,height=frame.padded_height;
  for(unsigned mip=0;mip<frame.views.size();++mip) {
    auto* pass=commands->beginComputePass();if(!pass)return s.fail("occlusion reduction pass failed");
    auto* root=pass->bindPipeline(s.pipelines[mip?4:3]);bool ok=root!=nullptr;
    if(root) {
      rhi::ShaderCursor c(root);ok=SLANG_SUCCEEDED(c["occlusionTarget"].setBinding(rhi::Binding(frame.views[mip])));
      if(mip)ok=ok&&SLANG_SUCCEEDED(c["occlusionSource"].setBinding(rhi::Binding(frame.views[mip-1])));
      else {
        const unsigned settings[]{s.capacity,s.input.width,s.input.height,0};
        ok=ok&&SLANG_SUCCEEDED(c["occlusionVisibility"].setBinding(rhi::Binding(visibility)))&&
            SLANG_SUCCEEDED(c["occlusionSettings"].setData(settings,sizeof(settings)));
      }
    }
    if(ok)pass->dispatchCompute((width+7)/8,(height+7)/8,1);
    pass->end();if(!ok)return s.fail("occlusion reduction binding failed");
    width=std::max(1u,width/2);height=std::max(1u,height/2);
  }
  commands->setTextureState(frame.pyramid,rhi::ResourceState::ShaderResource);s.built=true;return true;
}
bool OcclusionGpu::retest(rhi::ICommandEncoder* commands) {
  auto& s=*state_;if(!s.pending || !s.built || !commands)return s.fail("occlusion retest has no current depth");
  return s.classify(commands,2,s.frames[s.slot],s.input.view,true);
}
bool OcclusionGpu::finish(rhi::ICommandEncoder* commands,rhi::IBuffer* visibility) {
  auto& s=*state_;if(!build_current(commands,visibility))return false;
  auto& frame=s.frames[s.slot];frame.view=s.input.view;frame.valid=true;
  s.previous=s.slot;s.pending=false;return true;
}
}
