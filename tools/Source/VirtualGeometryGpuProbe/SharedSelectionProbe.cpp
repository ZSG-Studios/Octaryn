#include "SelectionGpu.h"
#include "SelectionResources.h"
#include "SceneMemoryLedger.h"
#include <slang-com-ptr.h>
#include <array>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace octaryn::client::rendering::virtual_geometry;
namespace {
void check(bool value,const char* text) {if(!value)throw std::runtime_error(text);}
SelectionTopology topology() {
  GeometryAsset asset;asset.pages.resize(2);asset.roots={0};asset.group_pages={0,1};
  asset.groups={{0,2,0,0,2,{{0,0,0},1,FLT_MAX}}};asset.clusters.resize(2);
  for(unsigned i=0;i<2;++i) {asset.clusters[i].page=i;asset.clusters[i].bounds={{0,0,0},1,0};}
  SelectionTopology result;std::string error;check(build_selection_topology(asset,result,error),error.c_str());return result;
}
}
bool probe_shared_selection(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* shader) {
  try {
    constexpr unsigned owners=32;
    auto ledger=std::make_shared<SceneMemoryLedger>(1024*1024);
    auto shared=std::make_shared<SelectionResources>();SelectionResourcesConfig config;
    config.groups=1;config.clusters=2;config.pages=2;config.page_references=2;config.parents=1;
    config.instances=2;config.feedback_capacity=2;config.tickets_per_frame=owners;
    config.readback_bytes=owners*SelectionResources::feedback_bytes(2,2);
    check(shared->initialize(device,ledger,shader,config),shared->error().c_str());
    const auto reserved=ledger->stats().used;
    check(reserved==SelectionResources::required_bytes(config),"shared selection physical allocation differs from budget");
    const auto source=topology();std::array<std::unique_ptr<SelectionGpu>,owners> selection;
    for(auto& owner:selection) {
      owner=std::make_unique<SelectionGpu>();check(owner->initialize(device,source,shader,2,2,shared),owner->error().c_str());
      check(owner->gpu_bytes()==0 && ledger->stats().used==reserved,"shared selection allocated private GPU storage");
    }
    {SelectionGpu overflow;check(!overflow.initialize(device,source,shader,2,2,shared),"feedback owner admission exceeded arena");}
    rhi::BufferDesc read{};read.size=owners*32;read.elementSize=4;read.usage=rhi::BufferUsage::CopyDestination;
    read.defaultState=rhi::ResourceState::CopyDestination;read.memoryType=rhi::MemoryType::ReadBack;
    auto output=device->createBuffer(read);check(bool(output),"shared selection result allocation");
    auto fence=device->createFence({});check(bool(fence),"shared selection fence");
    SelectionView view{{0,0,10},100,1};std::array<SelectionGpuFrame,owners> frames;
    std::uint64_t signal{};
    const auto record=[&](rhi::ICommandEncoder* commands,unsigned i) {
      const std::array<GpuPage,2> pages{{{i*2,100+i,1,16*i},{i*2+1,200+i,1,0}}};
      check(selection[i]->record(commands,pages,view,frames[i]),selection[i]->error().c_str());
      commands->copyBuffer(output,i*32,frames[i].selected,0,32);
    };
    const auto submit=[&](rhi::ICommandEncoder* encoder) {
      auto commands=encoder->finish();check(bool(commands),"shared selection commands");
      auto* command=commands.get();auto* f=fence.get();++signal;
      rhi::SubmitDesc desc{};desc.commandBuffers=&command;desc.commandBufferCount=1;
      desc.signalFences=&f;desc.signalFenceValues=&signal;desc.signalFenceCount=1;
      check(SLANG_SUCCEEDED(queue->submit(desc)),"shared selection submit");
      for(unsigned i=0;i<owners;++i)check(selection[i]->submitted(frames[i],fence,signal),selection[i]->error().c_str());
      check(SLANG_SUCCEEDED(device->waitForFences(1,&f,&signal,true,30'000'000'000ull)),"shared selection fence deadline");
      std::array<std::array<std::uint32_t,8>,owners> records{};
      void* mapped{};check(SLANG_SUCCEEDED(device->mapBuffer(output,rhi::CpuAccessMode::Read,&mapped)),"shared selection map results");
      std::memcpy(records.data(),mapped,sizeof(records));check(SLANG_SUCCEEDED(device->unmapBuffer(output)),"shared selection unmap results");
      for(unsigned i=0;i<owners;++i)for(unsigned cluster=0;cluster<2;++cluster) {
        const auto* value=records[i].data()+cluster*4;
        check(value[0]<2 && value[1]==value[0] && value[2]==i*2+value[0] &&
            value[3]==(value[0]?200:100)+i,"independent asset page IDs aliased shared selection scratch");
      }
    };
    auto encoder=queue->createCommandEncoder();for(unsigned i=0;i<owners;++i)record(encoder,i);submit(encoder);
    SelectionFeedback feedback;
    check(selection[0]->poll_feedback(feedback),"first shared feedback missing");
    // The first owner reuses GPU scratch before other owners consume their tagged CPU feedback.
    encoder=queue->createCommandEncoder();record(encoder,0);
    for(unsigned i=1;i<owners;++i) {
      check(selection[i]->poll_feedback(feedback),selection[i]->error().c_str());
      check(feedback.selected==2 && feedback.missing_roots==0 && feedback.used_pages==std::vector<std::uint32_t>({0,1}),
          "completed owner feedback overwritten during shared bank reuse");
      record(encoder,i);
    }
    submit(encoder);
    for(auto& owner:selection) {
      check(owner->poll_feedback(feedback) && feedback.selected==2,"second shared feedback missing");
      check(!owner->poll_feedback(feedback) && owner->error().empty(),"shared feedback consumed twice");
      owner.reset();
    }
    encoder.setNull();shared.reset();check(ledger->stats().used==0,"shared selection resource lease retained after owners release");
    std::printf("scene_selection_gpu passed=1 owners=%u frames=2 local_ids=isolated feedback=retained physical_bytes=%llu ledger_released=1\n",
        owners,static_cast<unsigned long long>(reserved));return true;
  }catch(const std::exception& error) {std::fprintf(stderr,"scene_selection_gpu failed=%s\n",error.what());return false;}
}
