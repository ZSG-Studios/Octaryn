#include "GeometryRootUpload.h"
#include "GeometryStream.h"
#include "../Rendering/RenderBackend/FrameWatchdog.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <algorithm>
#include <cstdio>

namespace octaryn::client::rendering::virtual_geometry {
bool upload_geometry_roots(GeometryStream& stream,rhi::IDevice* device,rhi::ICommandQueue* queue,std::string& error,const std::function<void()>& progress) {
  Slang::ComPtr<rhi::IFence> fence;
  if(!device || !queue || SLANG_FAILED(device->createFence({},fence.writeRef()))) {
    error="root upload fence creation failed";return false;
  }
  const auto start=std::chrono::steady_clock::now();
  auto batch_started=start;
  double maximum_iteration_ms{},maximum_batch_ms{};
  unsigned progress_batches{};
  std::uint64_t value{};
  while(!stream.roots_ready()) {
    if(std::chrono::steady_clock::now()-start>std::chrono::seconds(60)) {
      error="coarse geometry startup timed out";return false;
    }
    const auto iteration_started=std::chrono::steady_clock::now();
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
    submission=nullptr;commands=nullptr;
    const auto finished=std::chrono::steady_clock::now();
    maximum_iteration_ms=std::max(maximum_iteration_ms,
        std::chrono::duration<double,std::milli>(finished-iteration_started).count());
    const auto batch_ms=std::chrono::duration<double,std::milli>(finished-batch_started).count();
    maximum_batch_ms=std::max(maximum_batch_ms,batch_ms);
    if(progress && batch_ms>=16) {
      // Submitted work is fenced; the caller may present while this thread waits.
      progress();++progress_batches;batch_started=std::chrono::steady_clock::now();
    }
    if(!stream.roots_ready())SDL_Delay(1);
  }
  if(!stream.release_upload_timeline()) {error=stream.error();return false;}
  std::printf("geometry_root_upload_progress submissions=%llu batches=%u max_iteration_ms=%.3f max_batch_ms=%.3f elapsed_ms=%.3f\n",
      static_cast<unsigned long long>(value),progress_batches,maximum_iteration_ms,maximum_batch_ms,
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  std::fflush(stdout);
  return true;
}
}
