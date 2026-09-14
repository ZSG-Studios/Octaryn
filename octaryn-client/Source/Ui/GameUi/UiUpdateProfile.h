#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace octaryn::client::app {
// Opt-in wall-stage and thread-CPU evidence; report only after the UI has stopped.
class UiUpdateProfile {
public:
  static constexpr size_t StageCount=7;
  struct Sample {
    Uint64 update{},started{},total{},cpu{};
    std::array<Uint64,StageCount> stages{};
    bool refreshed{},cpu_valid{};
  };
  UiUpdateProfile():enabled_(SDL_getenv("OCTARYN_CLIENT_UI_PROFILE")!=nullptr) {}
  void begin() {
    if(!enabled_)return;
    current_={};current_.update=++updates_;
    cpu_started_=thread_cpu();
    current_.started=last_=SDL_GetTicksNS();
  }
  void mark(size_t stage) {
    if(!enabled_)return;
    const auto now=SDL_GetTicksNS();current_.stages[stage]=now-last_;last_=now;
  }
  void finish(bool refreshed) {
    if(!enabled_)return;
    current_.total=last_-current_.started;current_.refreshed=refreshed;
    const auto cpu=thread_cpu();
    current_.cpu_valid=cpu_started_!=~Uint64{} && cpu!=~Uint64{} && cpu>=cpu_started_;
    if(current_.cpu_valid)current_.cpu=cpu-cpu_started_;
    auto lowest=std::min_element(worst_.begin(),worst_.end(),
        [](const Sample& a,const Sample& b){return a.total<b.total;});
    if(current_.total>lowest->total)*lowest=current_;
  }
  void report() const {
    if(!enabled_)return;
    std::printf("ui_update_profile samples=%llu retained=8 thread_cpu=GetThreadTimes_when_available "
        "cpu_zero_may_be_below_os_timer_precision=1\n",static_cast<unsigned long long>(updates_));
    auto samples=worst_;
    std::sort(samples.begin(),samples.end(),[](const Sample& a,const Sample& b){return a.total>b.total;});
    for(const auto& s:samples)if(s.update) {
      std::printf("ui_update_slow update=%llu started_ns=%llu total_ms=%.3f thread_cpu_ms=%.3f "
          "cpu_available=%u refresh=%u viewport_ms=%.3f menu_ms=%.3f lighting_ms=%.3f capture_ms=%.3f "
          "telemetry_ms=%.3f context_ms=%.3f release_ms=%.3f\n",
          static_cast<unsigned long long>(s.update),static_cast<unsigned long long>(s.started),s.total/1e6,
          s.cpu/1e6,s.cpu_valid?1u:0u,s.refreshed?1u:0u,s.stages[0]/1e6,s.stages[1]/1e6,
          s.stages[2]/1e6,s.stages[3]/1e6,s.stages[4]/1e6,s.stages[5]/1e6,s.stages[6]/1e6);
    }
  }
private:
  static Uint64 thread_cpu() {
#ifdef _WIN32
    FILETIME created{},exited{},kernel{},user{};
    if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)) {
      const auto ticks=[](FILETIME value){return (Uint64(value.dwHighDateTime)<<32)|value.dwLowDateTime;};
      return (ticks(kernel)+ticks(user))*100;
    }
#endif
    return ~Uint64{};
  }
  bool enabled_{};
  Uint64 updates_{},last_{},cpu_started_{};
  Sample current_{};
  std::array<Sample,8> worst_{};
};
}
