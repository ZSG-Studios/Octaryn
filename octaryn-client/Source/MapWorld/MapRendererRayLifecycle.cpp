#include "MapRendererInternal.h"
#include "MapRayCompletion.h"
#include "FrameWatchdog.h"
#include <cstdio>

namespace octaryn::client::rendering {
namespace {
constexpr std::uint64_t completion_value=1;
void publish(MapRenderer& map) {
  map.ray_pending_commands.setNull();map.ray_pending_fence.setNull();
  const auto released=(map.blas_scratch?map.blas_scratch->getDesc().size:0)+
      (map.tlas_scratch?map.tlas_scratch->getDesc().size:0);
  map.blas_scratch.setNull();map.tlas_scratch.setNull();map.instances.setNull();
  map.ray_ready=true;
  std::printf("map_ray_enable_complete scratch_released_bytes=%llu\n",
      static_cast<unsigned long long>(released));std::fflush(stdout);
}
bool completed(MapRenderer& map) {
  std::uint64_t value{};
  const auto valid=SLANG_SUCCEEDED(map.ray_pending_fence->getCurrentValue(&value));
  const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-map.ray_submitted_at).count();
  const auto result=map_ray_completion(valid,value,elapsed);
  if(result==MapRayCompletion::Failed)
    frame_gpu_shutdown_failed("map_ray_enable_fence_status");
  return result==MapRayCompletion::Complete;
}
}
bool pump_map_ray_scene(MapRenderer& map,rhi::ICommandQueue* queue,bool requested) {
  // Poll even after disabling: submitted resources cannot be dropped in flight.
  if(map.ray_pending_fence) {
    if(completed(map))publish(map);
    return true;
  }
  if(!requested || map.ray_ready || !map.ray_supported)return true;
  if(!queue)return false;
  const auto start=std::chrono::steady_clock::now();
  std::puts("map_ray_enable_record_begin");std::fflush(stdout);
  Slang::ComPtr<rhi::IFence> fence;
  if(SLANG_FAILED(map.device->createFence({},fence.writeRef())))return false;
  auto commands=queue->createCommandEncoder();
  if(!commands || !prepare_map_ray_scene(map,commands))return false;
  auto submission=commands->finish();if(!submission)return false;
  rhi::ICommandBuffer* buffer=submission.get();rhi::IFence* signal=fence.get();
  rhi::SubmitDesc submit{};submit.commandBuffers=&buffer;submit.commandBufferCount=1;
  submit.signalFences=&signal;submit.signalFenceValues=&completion_value;submit.signalFenceCount=1;
  // DX12 can execute command lists before a later Signal makes submit fail.
  if(SLANG_FAILED(queue->submit(submit)))frame_gpu_shutdown_failed("map_ray_enable_submit");
  map.ray_pending_fence=fence;map.ray_pending_commands=submission;
  map.ray_submitted_at=std::chrono::steady_clock::now();
  std::printf("map_ray_enable_submitted owner_cpu_ms=%.3f\n",
      std::chrono::duration<double,std::milli>(map.ray_submitted_at-start).count());std::fflush(stdout);
  // Deliberately defer publication to a subsequent owner poll, even on fast GPUs.
  return true;
}
void finish_map_ray_scene(MapRenderer& map) {
  if(!map.ray_pending_fence)return;
  rhi::IFence* fence=map.ray_pending_fence.get();
  if(!completed(map) && SLANG_FAILED(map.device->waitForFences(1,&fence,&completion_value,true,1000000000ull)))
    frame_gpu_shutdown_failed("map_ray_enable_shutdown_wait");
  if(!completed(map))frame_gpu_shutdown_failed("map_ray_enable_shutdown_status");
  publish(map);
}
}
