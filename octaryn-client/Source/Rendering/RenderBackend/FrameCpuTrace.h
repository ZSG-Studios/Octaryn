#pragma once
#include "../../Threading/ThreadCpuTime.h"
#include <SDL3/SDL_thread.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace octaryn::client::rendering {
// One active interval; every boundary closes it before starting another.
class FrameCpuTrace {
public:
  explicit FrameCpuTrace(std::uint64_t frame):frame_(frame),enabled_(enabled()) {
    if(enabled_)thread_=SDL_GetCurrentThreadID();
  }
  ~FrameCpuTrace(){finish("unwind");}
  FrameCpuTrace(const FrameCpuTrace&)=delete;
  FrameCpuTrace& operator=(const FrameCpuTrace&)=delete;
  void begin(const char* stage) {
    if(!enabled_)return;
    finish("boundary");
    stage_=stage;start_=now();
    cpu_start_=threading::current_thread_cpu_nanoseconds();
  }
  void finish(const char* reason="return") {
    if(!enabled_ || !stage_)return;
    const auto end=now();
    const auto* stage=stage_;stage_=nullptr;
    if(end-start_<10'000'000)return;
    const auto cpu_end=threading::current_thread_cpu_nanoseconds();
    const auto cpu_ns=cpu_start_>=0 && cpu_end>=cpu_start_?cpu_end-cpu_start_:-1;
    // The next interval starts after logging, excluding diagnostic output latency.
    std::fprintf(stderr,"world_frame_cpu_stage renderer_frame=%llu app_frame=%llu stage=%s "
        "thread_id=%llu start_ns=%llu end_ns=%llu wall_ms=%.6f thread_cpu_ns=%lld thread_cpu_ms=%.6f end=%s\n",
        static_cast<unsigned long long>(frame_),static_cast<unsigned long long>(frame_+1),stage,
        static_cast<unsigned long long>(thread_),static_cast<unsigned long long>(start_),
        static_cast<unsigned long long>(end),double(end-start_)/1e6,
        static_cast<long long>(cpu_ns),cpu_ns>=0?double(cpu_ns)/1e6:-1.0,reason);
  }
  bool failed(){finish();return false;}
private:
  static bool enabled() {
    static const bool value=[] {
      const char* text=std::getenv("OCTARYN_CLIENT_FRAME_CPU_TRACE");
      return text && text[0]=='1' && text[1]=='\0';
    }();
    return value;
  }
  static std::uint64_t now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
  }
  std::uint64_t frame_{},thread_{},start_{};
  std::int64_t cpu_start_{-1};
  const char* stage_{};
  bool enabled_{};
};
}
