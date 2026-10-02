#include "OcclusionGpu.h"
#include "GeometryFormat.h"
#include <slang-com-ptr.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

using Slang::ComPtr;
using namespace octaryn::client::rendering::virtual_geometry;
namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
ComPtr<rhi::IBuffer> buffer(rhi::IDevice* device,std::size_t bytes,unsigned stride,const void* initial=nullptr) {
  rhi::BufferDesc desc{};desc.size=bytes;desc.elementSize=stride;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|
      rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  ComPtr<rhi::IBuffer> result;
  check(SLANG_SUCCEEDED(device->createBuffer(desc,initial,result.writeRef())),"bin fixture buffer allocation");return result;
}
void submit(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::ICommandEncoder* encoder) {
  auto commands=encoder->finish();check(bool(commands),"bin fixture command finish");
  auto fence=device->createFence({});check(bool(fence),"bin fixture fence");
  auto* command=commands.get();auto* signal=fence.get();std::uint64_t value=1;
  rhi::SubmitDesc desc{};desc.commandBuffers=&command;desc.commandBufferCount=1;
  desc.signalFences=&signal;desc.signalFenceValues=&value;desc.signalFenceCount=1;
  check(SLANG_SUCCEEDED(queue->submit(desc)),"bin fixture submission");
  check(SLANG_SUCCEEDED(device->waitForFences(1,&signal,&value,true,30'000'000'000ull)),"bin fixture fence timeout");
}
}
bool probe_occlusion_bins(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* shader) {
  try {
    constexpr unsigned capacity=96,extent=64;
    OcclusionGpu bins;check(bins.initialize(device,shader,capacity),bins.error().c_str());bins.set_history_enabled(false);
    std::array<GeometryCluster,capacity> clusters{};
    std::array<std::array<unsigned,4>,capacity> selected{};
    for(unsigned i=0;i<capacity;++i)selected[i]={i,0,0,1};
    auto clusterBuffer=buffer(device,sizeof(clusters),sizeof(GeometryCluster));
    auto selectionBuffer=buffer(device,sizeof(selected),16,selected.data());
    auto counters=buffer(device,24,4);
    std::vector<std::uint64_t> emptyVisibility(extent*extent);
    auto visibility=buffer(device,emptyVisibility.size()*8,8,emptyVisibility.data());
    // Copies made immediately after begin, before depth/retest/finish can repair stale args.
    auto recorded=buffer(device,56,4);
    OcclusionInputs input{clusterBuffer,selectionBuffer,counters,extent,extent};
    input.view[4]=input.view[9]=input.view[14]=1;
    input.view[16]=input.view[17]=input.view[18]=1;input.view[19]=.1f;
    unsigned cases{};
    const std::array<std::array<unsigned,2>,5> counts{{{1,0},{33,5},{64,31},{2,1},{0,0}}};
    for(const auto& count:counts) {
      const auto hardware=count[0],software=count[1];
      for(unsigned i=0;i<capacity;++i)clusters[i].bounds={{0,0,3},i<hardware?.5f:.02f,0};
      const std::array<unsigned,6> selectedCounts{hardware+software,0,0,0,0,0};
      auto encoder=queue->createCommandEncoder();check(bool(encoder),"bin fixture encoder");
      check(SLANG_SUCCEEDED(encoder->uploadBufferData(clusterBuffer,0,sizeof(clusters),clusters.data())),"bin fixture cluster upload");
      check(SLANG_SUCCEEDED(encoder->uploadBufferData(counters,0,sizeof(selectedCounts),selectedCounts.data())),"bin fixture counter upload");
      check(bins.begin(encoder,0,input),bins.error().c_str());
      if(cases!=0) {
        encoder->copyBuffer(recorded,0,bins.bin_args(),0,48);
        encoder->copyBuffer(recorded,48,bins.early_hardware(),0,4);
        encoder->copyBuffer(recorded,52,bins.early_software(),0,4);
      }
      check(bins.build_current(encoder,visibility)&&bins.retest(encoder)&&bins.finish(encoder,visibility),bins.error().c_str());
      submit(device,queue,encoder);
      // The first complete frame legitimately seeds the bank through retest.
      if(cases==0) {++cases;continue;}
      std::array<unsigned,14> words{};
      check(SLANG_SUCCEEDED(device->readBuffer(recorded,0,sizeof(words),words.data())),"bin fixture count readback");
      check(words[12]==hardware&&words[13]==software,"classification bin counts disagree with fixture");
      const std::array<unsigned,12> expected{(hardware+31)/32,1,1,software,1,1,0,1,1,0,1,1};
      if(!std::equal(expected.begin(),expected.end(),words.begin())) {
        std::fprintf(stderr,"geometry_occlusion_bins mismatch case=%u hardware=%u software=%u early_mesh_x=%u expected_mesh_x=%u early_software_x=%u expected_software_x=%u\n",
            cases,hardware,software,words[0],expected[0],words[3],expected[3]);
        throw std::runtime_error("early indirect records were not finalized from this frame's bins");
      }
      ++cases;
    }
    std::printf("geometry_occlusion_bins passed=1 changing_counts=%u warmup_frames=1 begin_records_current=1 growing_shrinking_zero=1 history=off\n",cases-1);
    return true;
  }catch(const std::exception& error) {std::fprintf(stderr,"geometry_occlusion_bins failed=%s\n",error.what());return false;}
}
