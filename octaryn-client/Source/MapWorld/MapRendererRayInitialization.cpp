#include "MapRendererInternal.h"
#include "FrameWatchdog.h"
#include <chrono>
#include <cstdio>

namespace octaryn::client::rendering {
bool initialize_map_ray_scene(MapRenderer& map,rhi::ICommandQueue* queue) {
  if(map.ray_ready || !map.ray_supported)return true;
  const auto start=std::chrono::steady_clock::now();
  const auto fail=[&](const char* stage) {
    map.ray_ready=false;
    std::fprintf(stderr,"map_ray_initialization_failed stage=%s elapsed_ms=%.3f\n",stage,
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    return false;
  };
  std::puts("map_ray_startup_begin");std::fflush(stdout);
  if(!queue)return fail("queue");
  Slang::ComPtr<rhi::IFence> fence;
  if(SLANG_FAILED(map.device->createFence({},fence.writeRef())))return fail("fence");
  auto commands=queue->createCommandEncoder();
  if(!commands)return fail("encoder");
  if(!prepare_map_ray_scene(map,commands))return fail("record");
  // Recording alone is not readiness: the static build must finish on the GPU.
  map.ray_ready=false;
  auto submission=commands->finish();
  if(!submission)return fail("finish");
  rhi::ICommandBuffer* command_buffer=submission.get();
  rhi::IFence* completed=fence.get();const std::uint64_t value=1;
  rhi::SubmitDesc submit{};submit.commandBuffers=&command_buffer;submit.commandBufferCount=1;
  submit.signalFences=&completed;submit.signalFenceValues=&value;submit.signalFenceCount=1;
  // A failed Signal after ExecuteCommandLists does not mean work was unsubmitted.
  if(SLANG_FAILED(queue->submit(submit))) {
    fail("submit");frame_gpu_shutdown_failed("map_ray_startup_submit");
  }
  if(SLANG_FAILED(map.device->waitForFences(1,&completed,&value,true,1000000000ull))) {
    fail("fence_timeout");frame_gpu_shutdown_failed("map_ray_startup_wait");
  }
  std::uint64_t signaled{};
  if(SLANG_FAILED(fence->getCurrentValue(&signaled)) || signaled==UINT64_MAX || signaled<value) {
    fail("fence_status");frame_gpu_shutdown_failed("map_ray_startup_status");
  }
  submission.setNull();commands.setNull();
  if(!submit_map_ray_compaction(map,queue,false))return fail("compaction");
  if(map.ray_pending_fence)finish_map_ray_scene(map);
  const auto released=(map.blas_scratch?map.blas_scratch->getDesc().size:0)+
      (map.tlas_scratch?map.tlas_scratch->getDesc().size:0);
  map.blas_scratch.setNull();map.tlas_scratch.setNull();map.instances.setNull();
  map.ray_ready=true;
  std::printf("map_ray_startup_end elapsed_ms=%.3f scratch_released_bytes=%llu\n",
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),
      static_cast<unsigned long long>(released));std::fflush(stdout);
  return true;
}
}
