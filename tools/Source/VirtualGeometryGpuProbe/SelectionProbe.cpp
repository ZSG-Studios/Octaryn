#include "SelectionGpu.h"
#include <slang-com-ptr.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace octaryn::client::rendering::virtual_geometry;
using Slang::ComPtr;
namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
GeometryAsset fixture() {
  GeometryAsset asset;asset.pages.resize(4);asset.groups.resize(3);
  asset.roots={1,2};asset.group_pages={0,1,2,3};
  asset.groups[0]={0,2,0,0,2,{{0,0,0},1,10}};
  asset.groups[1]={2,1,1,2,1,{{0,0,0},3,std::numeric_limits<float>::max()}};
  asset.groups[2]={3,1,1,3,1,{{0,0,0},3,std::numeric_limits<float>::max()}};
  asset.clusters.resize(4);
  asset.clusters[0].group=asset.clusters[1].group=0;
  asset.clusters[0].page=0;asset.clusters[1].page=1;
  asset.clusters[0].bounds={{-2,0,0},.5f,0};asset.clusters[1].bounds={{2,0,0},.5f,0};
  asset.clusters[2].group=1;asset.clusters[2].page=2;asset.clusters[2].refined_group=0;
  asset.clusters[3].group=2;asset.clusters[3].page=3;asset.clusters[3].refined_group=0;
  asset.clusters[2].bounds=asset.clusters[3].bounds={{0,0,0},3,0};
  return asset;
}
void submit(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::ICommandEncoder* encoder,
    SelectionGpu& selection,std::span<const SelectionGpuFrame> frames) {
  auto commands=encoder->finish();require(bool(commands),"selection finish");
  auto fence=device->createFence({});require(bool(fence),"selection fence");
  rhi::ICommandBuffer* command=commands.get();rhi::IFence* signal=fence.get();std::uint64_t value=1;
  rhi::SubmitDesc desc{};desc.commandBuffers=&command;desc.commandBufferCount=1;
  desc.signalFences=&signal;desc.signalFenceValues=&value;desc.signalFenceCount=1;
  require(SLANG_SUCCEEDED(queue->submit(desc)),"selection submit");
  for(const auto& frame:frames)require(selection.submitted(frame,fence,value),selection.error().c_str());
  require(SLANG_SUCCEEDED(device->waitForFences(1,&signal,&value,true,30'000'000'000ull)),"selection fence timeout");
}
}
bool probe_virtual_geometry_selection(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* shader) {
  try {
    auto asset=fixture();SelectionTopology topology;std::string error;
    require(build_selection_topology(asset,topology,error),error.c_str());
    SelectionGpu selection;require(selection.initialize(device,topology,shader,1,2),selection.error().c_str());
    SelectionView view{{0,0,10},100,1};std::vector<GpuPage> pages(4);
    unsigned cases=0;
    const auto compare=[&] {
      SelectionResult expected;const bool complete=select_geometry(topology,pages,view,4,1,expected,error);
      auto encoder=queue->createCommandEncoder();require(bool(encoder),"selection command encoder");
      SelectionGpuFrame frame;require(selection.record(encoder,pages,view,frame),selection.error().c_str());
      submit(device,queue,encoder,selection,std::span(&frame,1));
      SelectionFeedback feedback;require(selection.poll_feedback(feedback),selection.error().c_str());
      require(feedback.slot==frame.slot && feedback.generation==frame.generation,
          "completed selection feedback recording identity mismatch");
      std::array<unsigned,6> arguments{};
      require(SLANG_SUCCEEDED(device->readBuffer(frame.dispatch,0,sizeof(arguments),arguments.data())),"selection argument readback");
      require(!feedback.selected_overflow,"selection unexpectedly overflowed");
      require((feedback.missing_roots!=0)==!complete,"selection root coverage parity");
      require((feedback.feedback_overflow!=0)==(expected.feedback_overflow!=0),"selection feedback overflow parity");
      if(complete) {
        std::vector<std::array<unsigned,4>> records(feedback.selected);
        if(!records.empty())require(SLANG_SUCCEEDED(device->readBuffer(frame.selected,0,records.size()*16,records.data())),"selection result readback");
        std::vector<unsigned> selected;
        for(auto record:records) {
          require(record[0]<asset.clusters.size(),"selected cluster outside topology");
          const auto page=asset.clusters[record[0]].page;
          require(record[1]==page && record[2]==pages[page].slot && record[3]==pages[page].generation,"selection physical page handle mismatch");
          selected.push_back(record[0]);
        }
        std::sort(selected.begin(),selected.end());std::sort(expected.clusters.begin(),expected.clusters.end());
        require(selected==expected.clusters,"GPU cluster cut differs from CPU reference");
        require(arguments[0]==(selected.size()+31)/32 && arguments[3]==selected.size(),"GPU indirect counts mismatch");
      } else require(arguments[0]==0 && arguments[3]==0,"missing roots emitted partial draw");
      require(feedback.requests.size()<=1,"GPU feedback capacity exceeded");
      for(auto request:feedback.requests)require(request.page<pages.size() &&
          (!pages[request.page].resident || !pages[request.page].generation || pages[request.page].slot==invalid_page),
          "GPU feedback requested available/invalid page");
      ++cases;
    };
    compare();pages[2]={2,11,1,0};pages[3]={3,12,1,0};compare();
    pages[0]={0,13,1,0};compare();pages[1]={1,14,1,0};compare();
    view.error_pixels=200;compare();view.error_pixels=1;
    view.frustum=true;view.planes[0][0]=2;compare();view.frustum=false;
    pages[0].generation=0;compare();pages[0].generation=13;
    pages[0].resident=0;view.frustum=true;view.planes[0][3]=-20;compare();
    pages[0].resident=1;view.frustum=false;view.planes[0][3]=0;
    auto encoder=queue->createCommandEncoder();std::array<SelectionGpuFrame,2> frames{};
    for(auto& frame:frames)require(selection.record(encoder,pages,view,frame),selection.error().c_str());
    SelectionGpuFrame refused;require(!selection.record(encoder,pages,view,refused),"GPU frame ring overwrote pending work");
    submit(device,queue,encoder,selection,frames);
    SelectionFeedback feedback;
    require(selection.poll_feedback(feedback) && selection.poll_feedback(feedback),"GPU frame feedback ring drain");
    require(!selection.poll_feedback(feedback) && selection.error().empty(),"GPU frame feedback consumed twice");
    require(!selection.submitted(frames[0],nullptr,1),"stale GPU submission accepted");
    std::printf("geometry_selection_gpu passed=1 parity_cases=%u dag_shared_parents=1 feedback_bounds=1 generation=1 frustum=1 frame_ring=1\n",cases);
    return true;
  } catch(const std::exception& error) {std::fprintf(stderr,"geometry_selection_gpu failed=%s\n",error.what());return false;}
}
