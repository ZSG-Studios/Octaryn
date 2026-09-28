#pragma once
#include "FrameCpuProfile.h"
#include "../../Threading/ThreadCpuTime.h"
#include <SDL3/SDL_thread.h>
#include <chrono>
namespace octaryn::client::rendering {
class FrameCpuTrace {
  FrameCpuProfile* profile_{};
  const char* stage_{};
  const char* status_{"unwound"};
  std::uint64_t start_{};
  std::int64_t cpu_start_{};
  void boundary() {
    if(!profile_ || !stage_)return;
    const auto end=now();const auto cpu_end=threading::current_thread_cpu_nanoseconds();
    profile_->interval(stage_,start_,end,cpu_start_>=0 && cpu_end>=cpu_start_?cpu_end-cpu_start_:-1);
    stage_=nullptr;
  }
  bool publish() {
    if(!profile_)return true;
    boundary();auto* owner=profile_;profile_=nullptr;owner->finish(now(),status_);
    return owner->healthy();
  }
public:
  static std::uint64_t now() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
  }
  FrameCpuTrace(FrameCpuProfile& profile,std::uint64_t frame,const char* scope)
      :profile_(profile.enabled()?&profile:nullptr) {
    if(profile_)profile_->begin(frame,SDL_GetCurrentThreadID(),now(),scope);
  }
  ~FrameCpuTrace() {publish();}
  FrameCpuTrace(const FrameCpuTrace&)=delete;
  FrameCpuTrace& operator=(const FrameCpuTrace&)=delete;
  bool enabled() const {return profile_!=nullptr;}
  void begin(const char* stage) {
    if(!profile_)return;
    boundary();stage_=stage;start_=now();cpu_start_=threading::current_thread_cpu_nanoseconds();
  }
  void fence(const FrameFenceRecord& record) {if(profile_)profile_->fence(record);}
  void finish(const char* status="complete") {
    if(!std::strcmp(status_,"unwound") || std::strcmp(status,"complete"))status_=status;
  }
  bool failed() {status_="failed";return false;}
  bool complete(const char* status="complete") {finish(status);return publish();}
};
}
