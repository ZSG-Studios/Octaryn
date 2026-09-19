#include "FrameWatchdog.h"
#include <cstdlib>
#include <cstdio>
namespace octaryn::client::rendering {
namespace {
std::uint64_t env_ms(const char* name,std::uint64_t fallback) {
  const char* text=std::getenv(name);
  if(!text || !*text)return fallback;
  const long value=std::atol(text);
  if(value<0)return fallback;
  return static_cast<std::uint64_t>(value);
}
}
std::uint64_t frame_fence_timeout_ms() {
  static const std::uint64_t value=env_ms("OCTARYN_CLIENT_FENCE_TIMEOUT_MS",8000);
  return value;
}
std::uint64_t frame_watchdog_ms() {
  static const std::uint64_t value=env_ms("OCTARYN_CLIENT_FRAME_WATCHDOG_MS",5000);
  return value;
}
[[noreturn]] void frame_gpu_shutdown_failed(const char* stage) {
  std::fprintf(stderr,"world_gpu_shutdown_failed stage=%s; terminating without GPU resource teardown\n",stage);
  std::fflush(nullptr);
  std::_Exit(EXIT_FAILURE);
}
}
