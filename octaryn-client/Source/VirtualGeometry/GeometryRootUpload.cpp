#include "GeometryRootUpload.h"
#include "GeometryStream.h"
#include "../Rendering/RenderBackend/FrameWatchdog.h"
#include <SDL3/SDL.h>
#include <chrono>

namespace octaryn::client::rendering::virtual_geometry {
bool upload_geometry_roots(GeometryStream& stream,rhi::IDevice* device,rhi::ICommandQueue* queue,std::string& error) {
  Slang::ComPtr<rhi::IFence> fence;
  if(!device || !queue || SLANG_FAILED(device->createFence({},fence.writeRef()))) {
    error="root upload fence creation failed";return false;
  }
  const auto start=std::chrono::steady_clock::now();
  std::uint64_t value{};
  while(!stream.roots_ready()) {
    if(std::chrono::steady_clock::now()-start>std::chrono::seconds(60)) {
      error="coarse geometry startup timed out";return false;
    }
    auto commands=queue->createCommandEncoder();
    if(!commands) {error="root upload encoder failed";return false;}
    if(!stream.pump(commands,{})) {error=stream.error();return false;}
    auto submission=commands->finish();
    if(!submission) {error="root upload command finish failed";return false;}
    auto* buffer=submission.get();auto* signal=fence.get();++value;
    rhi::SubmitDesc submit{};submit.commandBuffers=&buffer;submit.commandBufferCount=1;
    submit.signalFences=&signal;submit.signalFenceValues=&value;submit.signalFenceCount=1;
    if(SLANG_FAILED(queue->submit(submit)))frame_gpu_shutdown_failed("geometry_root_upload_submit");
    if(SLANG_FAILED(device->waitForFences(1,&signal,&value,true,frame_fence_timeout_ms()*1000000ull)))
      frame_gpu_shutdown_failed("geometry_root_upload_fence");
    if(!stream.submitted(fence,value)) {error=stream.error();return false;}
    if(!stream.roots_ready())SDL_Delay(1);
  }
  if(!stream.release_upload_timeline()) {error=stream.error();return false;}
  return true;
}
}
