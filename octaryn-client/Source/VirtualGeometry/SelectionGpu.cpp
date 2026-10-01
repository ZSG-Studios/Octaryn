#include "SelectionGpu.h"
#include "InstanceSelection.h"
#include "SelectionResourcesInternal.h"
#include "../Rendering/RenderBackend/RhiShader.h"
#include <slang-com-ptr.h>
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace octaryn::client::rendering::virtual_geometry {
using Slang::ComPtr;
struct SelectionGpu::State {
  struct Frame {
    ComPtr<rhi::IBuffer> pages,active,requested,priorities,feedback,selected,counters,dispatch,readback,used,instances;
    ComPtr<rhi::IFence> fence;
    std::uint64_t signal{};
    std::uint32_t generation{},instance_count{};
    std::uint64_t readback_offset{};
    std::shared_ptr<SharedSelectionTicket> ticket;
    bool recorded{},consumed{true};
  };
  std::shared_ptr<SelectionResources> shared;
  ComPtr<rhi::IDevice> device;
  ComPtr<rhi::IBuffer> groups,clusters,group_pages,parents;
  std::array<ComPtr<rhi::IComputePipeline>,5> pipelines;
  std::vector<Frame> frames;
  SelectionTopology topology;
  std::uint64_t owner{};
  std::uint32_t group_count{},page_count{},cluster_count{},depth{},capacity{};
  std::string error;
  bool admission_rejected{};
  ~State() {if(shared && owner)shared->unregister_owner(owner);}
  bool fail(const char* value) {error=value;return false;}
  bool buffer(ComPtr<rhi::IBuffer>& output,std::uint64_t bytes,unsigned stride,const void* data=nullptr,bool readback=false) {
    rhi::BufferDesc desc{};desc.size=std::max<std::uint64_t>(bytes,stride);desc.elementSize=stride;
    desc.usage=readback?rhi::BufferUsage::CopyDestination:
        rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopyDestination|
        rhi::BufferUsage::CopySource|rhi::BufferUsage::IndirectArgument;
    desc.defaultState=readback?rhi::ResourceState::CopyDestination:rhi::ResourceState::ShaderResource;
    if(readback)desc.memoryType=rhi::MemoryType::ReadBack;
    return SLANG_SUCCEEDED(device->createBuffer(desc,data,output.writeRef())) || fail("selection buffer allocation failed");
  }
  bool dispatch_pass(rhi::ICommandEncoder* commands,Frame& f,unsigned pipeline,unsigned count,
      unsigned current_depth,const SelectionView& view) {
    auto* pass=commands->beginComputePass();if(!pass)return fail("selection compute pass failed");
    auto* root=pass->bindPipeline(pipelines[pipeline]);bool ok=root!=nullptr;
    if(root) {
      rhi::ShaderCursor cursor(root);
      const auto binding=[&](const char* name,rhi::IBuffer* value) {
        auto field=cursor[name];return !field.isValid() || SLANG_SUCCEEDED(field.setBinding(rhi::Binding(value)));
      };
      ok=binding("geometryGroups",groups)&&binding("geometryClusters",clusters)&&
         binding("geometryGroupPages",group_pages)&&binding("geometryParents",parents)&&
         binding("geometryPages",f.pages)&&binding("geometryActiveGroups",f.active)&&
         binding("geometryRequestedPages",f.requested)&&binding("geometryRequestPriorities",f.priorities)&&
         binding("geometryFeedback",f.feedback)&&binding("geometrySelected",f.selected)&&
         binding("geometryCounters",f.counters)&&binding("geometryDispatch",f.dispatch)&&
         binding("geometryUsedPages",f.used)&&binding("geometryInstances",f.instances);
      const unsigned counts[4]={group_count,page_count,cluster_count,current_depth},limits[2]={cluster_count,capacity};
      const float eye[4]={view.eye[0],view.eye[1],view.eye[2],view.focal_pixels};
      const unsigned frustum=view.frustum?1u:0u;
      const auto data=[&](const char* name,const void* value,std::size_t size) {
        auto field=cursor[name];return !field.isValid() || SLANG_SUCCEEDED(field.setData(value,size));
      };
      ok=ok&&data("geometryCounts",counts,sizeof(counts))&&data("geometryLimits",limits,sizeof(limits))&&
          data("geometryFrame",&f.generation,sizeof(f.generation))&&
          data("geometryInstanceCount",&f.instance_count,sizeof(f.instance_count))&&
          data("geometryEyeFocal",eye,sizeof(eye))&&data("geometryErrorPixels",&view.error_pixels,sizeof(float))&&
          data("geometryPlanes",view.planes,sizeof(view.planes))&&data("geometryFrustum",&frustum,sizeof(frustum));
    }
    const auto groups=pipeline==3?1u:count/64+unsigned(count%64!=0);
    if(ok)pass->dispatchCompute(std::min(groups,65535u),std::max(1u,groups/65535+unsigned(groups%65535!=0)),1);
    pass->end();commands->globalBarrier();return ok || fail("selection shader binding failed");
  }
};
SelectionGpu::SelectionGpu():state_(std::make_unique<State>()) {}
SelectionGpu::~SelectionGpu()=default;
const std::string& SelectionGpu::error() const {return state_->error;}
bool SelectionGpu::admission_rejected() const {return state_->admission_rejected;}
std::uint64_t SelectionGpu::gpu_bytes() const {
  const auto& s=*state_;std::uint64_t bytes{};
  if(s.shared)return 0;
  const auto add=[&](rhi::IBuffer* buffer){if(buffer)bytes+=buffer->getDesc().size;};
  add(s.groups);add(s.clusters);add(s.group_pages);add(s.parents);
  for(const auto& frame:s.frames)for(auto* buffer:{frame.pages.get(),frame.active.get(),frame.requested.get(),
      frame.priorities.get(),frame.feedback.get(),frame.selected.get(),frame.counters.get(),frame.dispatch.get(),frame.used.get(),frame.readback.get(),frame.instances.get()})add(buffer);
  return bytes;
}
bool SelectionGpu::initialize(rhi::IDevice* device,const SelectionTopology& topology,const char* path,
    std::uint32_t capacity,std::uint32_t frame_count,std::shared_ptr<SelectionResources> shared) {
  auto& s=*state_;
  if(s.device || !device || !path || !capacity || !frame_count || frame_count>8 ||
      topology.groups.empty() || topology.clusters.empty() || topology.pages.empty() ||
      topology.maximum_depth>=topology.groups.size())return s.fail("invalid selection initialization");
  s.device=device;s.capacity=capacity;s.depth=topology.maximum_depth;
  s.group_count=static_cast<unsigned>(topology.groups.size());s.cluster_count=static_cast<unsigned>(topology.clusters.size());
  const auto maximum=*std::max_element(topology.pages.begin(),topology.pages.end());
  if(maximum==invalid_id)return s.fail("selection page count overflow");s.page_count=maximum+1;
  if(shared) {
    if(shared->device()!=device)return s.fail("shared selection device differs");
    if(!shared->supports(topology,capacity)) {s.admission_rejected=true;return s.fail("selection asset exceeds shared topology capacity");}
    s.shared=std::move(shared);s.topology=topology;s.owner=s.shared->register_owner(s.page_count,capacity);
    if(!s.owner) {s.admission_rejected=true;return s.fail(s.shared->error().c_str());}
    s.frames.resize(8);s.error.clear();return true;
  }
  const auto upload=[&](auto& buffer,const auto& data) {
    using Value=typename std::decay_t<decltype(data)>::value_type;
    Value empty{};return s.buffer(buffer,std::max<std::size_t>(1,data.size())*sizeof(Value),sizeof(Value),data.empty()?&empty:data.data());
  };
  if(!upload(s.groups,topology.groups)||!upload(s.clusters,topology.clusters)||
      !upload(s.group_pages,topology.pages)||!upload(s.parents,topology.parents))return false;
  const char* names[] = {"reset_main","select_main","compact_main","finish_main","feedback_main"};
  for(unsigned i=0;i<s.pipelines.size();++i)
    if(!create_rhi_compute_pipeline(device,path,names[i],s.pipelines[i]))return s.fail("selection shader compilation failed");
  s.frames.resize(frame_count);
  for(auto& f:s.frames) {
    if(!s.buffer(f.pages,std::uint64_t(s.page_count)*16,16)||!s.buffer(f.active,std::uint64_t(s.group_count)*4,4)||
        !s.buffer(f.requested,std::uint64_t(s.page_count)*4,4)||!s.buffer(f.priorities,std::uint64_t(s.page_count)*4,4)||
        !s.buffer(f.feedback,std::uint64_t(capacity)*8,8)||!s.buffer(f.selected,std::uint64_t(s.cluster_count)*16,16)||
        !s.buffer(f.counters,24,4)||!s.buffer(f.dispatch,24,4)||!s.buffer(f.used,std::uint64_t(s.page_count)*4,4)||
        !s.buffer(f.readback,24+std::uint64_t(capacity)*8+std::uint64_t(s.page_count)*4,4,nullptr,true)||
        !s.buffer(f.instances,sizeof(InstanceSelectionView),sizeof(InstanceSelectionView)))return false;
  }
  s.error.clear();return true;
}
bool SelectionGpu::record(rhi::ICommandEncoder* commands,std::span<const GpuPage> pages,
    const SelectionView& view,SelectionGpuFrame& output,rhi::IQueryPool* timing,std::uint32_t first_query) {
  return record_impl(commands,pages,view,{},output,timing,first_query);
}
bool SelectionGpu::record(rhi::ICommandEncoder* commands,std::span<const GpuPage> pages,
    std::span<const InstanceSelectionView> views,SelectionGpuFrame& output,rhi::IQueryPool* timing,std::uint32_t first_query) {
  if(!valid_instance_selection(views))return state_->fail("invalid GPU instance selection views");
  return record_impl(commands,pages,SelectionView{},views,output,timing,first_query);
}
bool SelectionGpu::record_impl(rhi::ICommandEncoder* commands,std::span<const GpuPage> pages,
    const SelectionView& view,std::span<const InstanceSelectionView> views,SelectionGpuFrame& output,
    rhi::IQueryPool* timing,std::uint32_t first_query) {
  auto& s=*state_;output={};s.error.clear();
  if(!s.device || !commands || pages.size()!=s.page_count || !std::isfinite(view.focal_pixels) || view.focal_pixels<=0 ||
      !std::isfinite(view.error_pixels) || view.error_pixels<0)return s.fail("invalid selection recording");
  for(float value:view.eye)if(!std::isfinite(value))return s.fail("invalid selection eye");
  if(view.frustum)for(const auto& plane:view.planes)for(float value:plane)
    if(!std::isfinite(value))return s.fail("invalid selection frustum");
  unsigned first=0,last=unsigned(s.frames.size());
  if(s.shared) {
    SharedSelectionRecording acquired;
    const bool ok=s.shared->record(commands,s.owner,s.topology,s.capacity,unsigned(views.size()),acquired);
    if(!acquired.ticket)return s.fail(s.shared->error().c_str());
    first=acquired.ticket->slot;last=first+1;
    auto& f=s.frames[first];
    if(f.recorded || !f.consumed)return s.fail("shared selection owner still retains its previous frame");
    const auto& b=*acquired.buffers;
    s.groups=b.groups;s.clusters=b.clusters;s.group_pages=b.group_pages;s.parents=b.parents;s.pipelines=*acquired.pipelines;
    f.pages=b.pages;f.active=b.active;f.requested=b.requested;f.priorities=b.priorities;f.feedback=b.feedback;
    f.selected=b.selected;f.counters=b.counters;f.dispatch=b.dispatch;f.readback=b.readback;f.used=b.used;f.instances=b.instances;
    f.ticket=acquired.ticket;f.readback_offset=f.ticket->offset;f.generation=f.ticket->generation;
    if(!ok) {
      f.recorded=true;f.consumed=false;output={first,f.generation,f.selected,f.counters,f.dispatch,f.pages,s.owner};
      return s.fail(s.shared->error().c_str());
    }
  }
  for(unsigned slot=first;slot<last;++slot) {
    auto& f=s.frames[slot];if(f.recorded || !f.consumed || (!s.shared && f.generation==UINT32_MAX))continue;
    if(!s.shared && f.fence) {
      std::uint64_t value{};
      if(SLANG_FAILED(f.fence->getCurrentValue(&value)) || value==UINT64_MAX)return s.fail("selection frame fence failed");
      if(value<f.signal)continue;
    }
    if(views.size_bytes()>f.instances->getDesc().size) {
      if(s.shared)return s.fail("shared selection instance capacity exceeded");
      if(!s.buffer(f.instances,views.size_bytes(),sizeof(InstanceSelectionView)))return false;
    }
    f.instance_count=static_cast<std::uint32_t>(views.size());
    f.recorded=true;f.consumed=false;if(!s.shared)++f.generation;
    output={slot,f.generation,f.selected,f.counters,f.dispatch,f.pages,s.owner};
    if(!views.empty() && SLANG_FAILED(commands->uploadBufferData(f.instances,0,views.size_bytes(),views.data())))
      return s.fail("selection instance view upload failed");
    if(SLANG_FAILED(commands->uploadBufferData(f.pages,0,pages.size_bytes(),pages.data())))return s.fail("selection page table upload failed");
    if(timing)commands->writeTimestamp(timing,first_query);
    if(!s.dispatch_pass(commands,f,0,std::max({s.group_count,s.page_count,6u}),0,view))return false;
    if(timing)commands->writeTimestamp(timing,first_query+1);
    for(unsigned depth=s.depth+1;depth>0;--depth)
      if(!s.dispatch_pass(commands,f,1,s.group_count,depth-1,view))return false;
    if(timing)commands->writeTimestamp(timing,first_query+2);
    if(!s.dispatch_pass(commands,f,2,s.cluster_count,0,view))return false;
    if(timing)commands->writeTimestamp(timing,first_query+3);
    if(!s.dispatch_pass(commands,f,3,1,0,view)||!s.dispatch_pass(commands,f,4,s.capacity,0,view))return false;
    if(timing)commands->writeTimestamp(timing,first_query+4);
    commands->copyBuffer(f.readback,f.readback_offset,f.counters,0,24);
    commands->copyBuffer(f.readback,f.readback_offset+24,f.feedback,0,std::uint64_t(s.capacity)*8);
    commands->copyBuffer(f.readback,f.readback_offset+24+std::uint64_t(s.capacity)*8,f.used,0,std::uint64_t(s.page_count)*4);
    if(timing)commands->writeTimestamp(timing,first_query+5);
    // Each copy, shader, or indirect consumer requests its own required state.
    // Speculative transitions duplicate barriers when shared scratch is reused.
    return true;
  }
  return s.fail("selection frame ring busy; poll completed feedback");
}
bool SelectionGpu::submitted(const SelectionGpuFrame& frame,rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;
  if(frame.slot>=s.frames.size() || !fence || !value)return s.fail("invalid selection submission");
  auto& f=s.frames[frame.slot];
  if(!f.recorded || f.generation!=frame.generation || frame.owner!=s.owner)return s.fail("stale selection submission");
  if(s.shared && !s.shared->submitted(f.ticket,fence,value))return s.fail(s.shared->error().c_str());
  f.fence=fence;f.signal=value;f.recorded=false;return true;
}
bool SelectionGpu::poll_feedback(SelectionFeedback& output) {
  auto& s=*state_;output={};s.error.clear();
  for(auto& f:s.frames) {
    if(f.recorded || f.consumed || !f.fence)continue;
    if(s.shared) {
      if(!f.ticket || f.ticket->owner!=s.owner)return s.fail("shared selection feedback owner mismatch");
      if(!s.shared->completed(f.ticket)) {
        if(!s.shared->error().empty())return s.fail(s.shared->error().c_str());
        continue;
      }
    }
    std::uint64_t value{};
    if(!s.shared && (SLANG_FAILED(f.fence->getCurrentValue(&value)) || value==UINT64_MAX))return s.fail("selection feedback fence failed");
    if(!s.shared && value<f.signal)continue;
    void* data=nullptr;
    if(s.shared)data=f.ticket->feedback.data();
    else if(SLANG_FAILED(s.device->mapBuffer(f.readback,rhi::CpuAccessMode::Read,&data)) || !data)return s.fail("selection feedback mapping failed");
    const auto* words=static_cast<const std::uint32_t*>(data);
    output.selected=words[0];output.feedback_overflow=words[2];output.selected_overflow=words[3];output.missing_roots=words[4];
    output.maximum_error_pixels=std::bit_cast<float>(words[5]);
    const auto count=std::min(words[1],s.capacity);output.requests.reserve(count);
    for(unsigned i=0;i<count;++i) {
      const auto page=words[6+i*2];const auto priority=std::bit_cast<float>(words[7+i*2]);
      if(page<s.page_count && std::isfinite(priority) && priority>=0)output.requests.push_back({page,priority});
    }
    const auto* used=words+6+std::uint64_t(s.capacity)*2;
    for(unsigned page=0;page<s.page_count;++page)if(used[page])output.used_pages.push_back(page);
    const bool malformed=words[1]>s.capacity || words[0]>s.cluster_count ||
        !std::isfinite(output.maximum_error_pixels) || output.maximum_error_pixels<0;
    const auto result=s.shared?SLANG_OK:s.device->unmapBuffer(f.readback);f.consumed=true;
    if(f.ticket) {f.ticket->consumed=true;f.ticket.reset();}
    if(SLANG_FAILED(result) || malformed)return s.fail("invalid selection feedback result");
    return true;
  }
  return false;
}
}
