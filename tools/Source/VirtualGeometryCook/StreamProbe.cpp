#include "GeometryStream.h"
#include "MapTextureCache.h"
#include <slang-com-ptr.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <thread>

using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
using Slang::ComPtr;
namespace {
void check(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
struct Debug: rhi::IDebugCallback {
  unsigned errors{};
  SLANG_NO_THROW void SLANG_MCALL handleMessage(rhi::DebugMessageType type,rhi::DebugMessageSource,const char* message) override {
    if(type==rhi::DebugMessageType::Error)++errors;std::fprintf(stderr,"rhi: %s\n",message);
  }
};
struct Owner {
  ComPtr<rhi::ICommandQueue> queue;
  GeometryStream stream;
  ~Owner() {if(queue)queue->waitOnHost();}
};
}
int main(int argc,char** argv) {
  try {
    check(argc==5 || argc==6,"usage: virtual_geometry_stream_probe dx12|vulkan cache.vgeom source.glb slots [timeout-seconds]");
    check(!std::strcmp(argv[1],"dx12") || !std::strcmp(argv[1],"vulkan"),"invalid backend");
    std::string error;const auto hash=std::strcmp(argv[3],"-")==0?
        map_texture_digest(std::span<const std::uint8_t>{}):map_texture_file_digest(argv[3],error);
    check(!hash.empty(),error);
    GeometryAsset manifest;check(read_geometry_cache(argv[2],hash,manifest,error,false),error);
    std::set<unsigned> roots;
    for(auto root:manifest.roots) {
      const auto& group=manifest.groups[root];
      roots.insert(manifest.group_pages.begin()+group.first_page,manifest.group_pages.begin()+group.first_page+group.page_count);
    }
    const unsigned requested=unsigned(std::strtoul(argv[4],nullptr,10));
    const unsigned slots=requested?requested:unsigned(roots.size()+4);
    std::printf("geometry_stream_probe preflight root_pages=%zu total_pages=%zu slots=%u\n",roots.size(),manifest.pages.size(),slots);std::fflush(stdout);
    check(slots>roots.size() && slots<manifest.pages.size(),"probe needs roots < slots < total pages; 0 selects roots+4");
    Debug debug;rhi::DeviceDesc desc{};
    rhi::DebugLayerOptions validation{};validation.coreValidation=true;validation.required=true;
    check(SLANG_SUCCEEDED(rhi::getRHI()->setDebugLayerOptions(validation)),"enable GPU core validation failed");
    desc.deviceType=!std::strcmp(argv[1],"dx12")?rhi::DeviceType::D3D12:rhi::DeviceType::Vulkan;
    desc.enableValidation=true;desc.debugCallback=&debug;
    ComPtr<rhi::IDevice> device;check(SLANG_SUCCEEDED(rhi::getRHI()->createDevice(desc,device.writeRef())),"stream probe device creation failed");
    Owner owner;owner.queue=device->getQueue(rhi::QueueType::Graphics);check(bool(owner.queue),"stream probe queue creation failed");
    auto& stream=owner.stream;GeometryStreamConfig config;config.slots=slots;
    config.feedback_capacity=std::max(4096u,unsigned(roots.size()));config.workers=4;config.upload_pages=4;config.upload_ms=2;
    check(stream.initialize(device,argv[2],hash,config),stream.error());
    auto fence=device->createFence({});check(bool(fence),"stream probe fence creation failed");
    std::uint64_t signal=0;unsigned maximum_resident=0,publication_checks=0;
    const auto start=std::chrono::steady_clock::now();const double timeout=argc==6?std::strtod(argv[5],nullptr):120;
    check(std::isfinite(timeout) && timeout>0 && timeout<=300,"invalid probe timeout");
    const auto frame=[&](std::span<const PageRequest> requests) {
      check(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<timeout,"stream probe deadline exceeded");
      auto commands=owner.queue->createCommandEncoder();check(bool(commands),"stream probe command encoder failed");
      check(stream.pump(commands,requests),stream.error());
      const auto stats=stream.stats();maximum_resident=std::max(maximum_resident,stats.residency.resident);
      check(stats.residency.bytes<=std::uint64_t(slots)*page_bytes,"stream exceeded slot byte budget");
      if(stats.uploaded_this_pump) {
        check(stats.residency.pending>=stats.uploaded_this_pump,"page published before upload fence");++publication_checks;
      }
      auto finished=commands->finish();check(bool(finished),"stream probe command finish failed");
      rhi::ICommandBuffer* buffer=finished.get();rhi::IFence* timeline=fence.get();++signal;
      rhi::SubmitDesc submission{};submission.commandBuffers=&buffer;submission.commandBufferCount=1;
      submission.signalFences=&timeline;submission.signalFenceValues=&signal;submission.signalFenceCount=1;
      check(SLANG_SUCCEEDED(owner.queue->submit(submission)),"stream probe submit failed");
      check(stream.submitted(fence,signal),stream.error());
      check(SLANG_SUCCEEDED(device->waitForFences(1,&timeline,&signal,true,30'000'000'000ull)),"stream probe GPU fence timeout");
      if(signal%256==0) {
        std::printf("geometry_stream_progress frames=%llu resident=%u decoded=%llu\n",static_cast<unsigned long long>(signal),
            stats.residency.resident,static_cast<unsigned long long>(stats.decoded_pages));std::fflush(stdout);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    };
    while(!stream.roots_ready())frame({});
    const auto compare=[&](unsigned page) {
      check(stream.residency().resident(page),"stream page not resident for GPU comparison");
      std::vector<std::uint8_t> decoded,gpu(page_bytes);
      check(read_geometry_page(argv[2],manifest.pages[page],decoded,error),error);
      const auto handle=stream.residency().handle(page);
      check(SLANG_SUCCEEDED(device->readBuffer(stream.pool(),std::uint64_t(handle.slot)*page_bytes,page_bytes,gpu.data())),"stream GPU readback failed");
      check(decoded==gpu,"stream uploaded bytes differ from verified decoded page");
    };
    compare(*roots.begin());compare(*roots.rbegin());
    unsigned loaded=0;const unsigned needed=slots-unsigned(roots.size())+2;
    for(unsigned page=0;page<manifest.pages.size() && loaded<needed;++page)if(!roots.contains(page)) {
      const PageRequest request{page,1};
      while(!stream.residency().resident(page))frame(std::span(&request,1));
      compare(page);++loaded;
      check(stream.roots_ready(),"root page evicted during fine refinement");
    }
    const auto stats=stream.stats();
    check(loaded==needed && stats.residency.evictions>=2,"stream did not exercise bounded page eviction");
    check(maximum_resident<=slots && maximum_resident<manifest.pages.size(),"probe made all pages resident");
    check(publication_checks>0 && stream.gpu_idle(),"stream fence publication was not exercised");
    check(debug.errors==0,"stream RHI validation reported errors");
    std::printf("geometry_stream_probe passed=1 backend=%s roots=%zu slots=%u total_pages=%zu fine_pages=%u evictions=%llu gpu_page_parity=1 bounded_workers=4 fence_publication=1\n",
        argv[1],roots.size(),slots,manifest.pages.size(),loaded,static_cast<unsigned long long>(stats.residency.evictions));return 0;
  }catch(const std::exception& failure) {std::fprintf(stderr,"geometry_stream_probe failed: %s\n",failure.what());return 1;}
}
