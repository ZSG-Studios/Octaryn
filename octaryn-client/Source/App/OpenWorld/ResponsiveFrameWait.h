#pragma once
#include <chrono>
#include <cstdint>
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace octaryn::client::app {
enum class FrameWaitResult { Signaled, Timeout, Failed };
struct FrameWaitReport {
  std::uint64_t requested_ns{},actual_ns{};
  unsigned waits{},last_timeout_ms{};
  FrameWaitResult result{FrameWaitResult::Signaled};
};

// A timeout bounds each timer wait; the monotonic deadline still owns the cap.
// OS thread descheduling can exceed that timeout and remains visible in telemetry.
template<class Now,class Wait>
bool wait_frame_deadline(std::uint64_t duration_ns,Now now,Wait wait,FrameWaitReport& report) {
  report={};report.requested_ns=duration_ns;
  if(duration_ns>1'000'000'000ull)return false;
  const auto start=now();auto current=start;
  while(current-start<duration_ns) {
    const auto remaining=duration_ns-(current-start);
    report.last_timeout_ms=unsigned((remaining+999'999)/1'000'000);
    if(++report.waits>4096)return false;
    report.result=wait(remaining,report.last_timeout_ms);
    current=now();
    if(current<start)return false;
    report.actual_ns=current-start;
    if(report.result==FrameWaitResult::Failed)return false;
  }
  return true;
}

class ResponsiveFrameWait {
#if defined(_WIN32)
  HANDLE timer_{};
#endif
public:
  ~ResponsiveFrameWait() {
#if defined(_WIN32)
    if(timer_)CloseHandle(timer_);
#endif
  }
  ResponsiveFrameWait()=default;
  ResponsiveFrameWait(const ResponsiveFrameWait&)=delete;
  ResponsiveFrameWait& operator=(const ResponsiveFrameWait&)=delete;
  template<class Now>
  bool sleep(std::uint64_t duration_ns,Now now,FrameWaitReport& report) {
    return wait_frame_deadline(duration_ns,now,[this](std::uint64_t remaining,unsigned timeout_ms) {
#if defined(_WIN32)
      if(!timer_) {
        timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_MODIFY_STATE|SYNCHRONIZE);
        if(!timer_)timer_=CreateWaitableTimerW(nullptr,FALSE,nullptr);
        if(!timer_)return FrameWaitResult::Failed;
      }
      LARGE_INTEGER due{};due.QuadPart=-static_cast<LONGLONG>((remaining+99)/100);
      if(!SetWaitableTimerEx(timer_,&due,0,nullptr,nullptr,nullptr,0))return FrameWaitResult::Failed;
      const auto result=WaitForSingleObject(timer_,timeout_ms);
      if(result==WAIT_OBJECT_0)return FrameWaitResult::Signaled;
      if(result==WAIT_TIMEOUT)return FrameWaitResult::Timeout;
      return FrameWaitResult::Failed;
#else
      (void)timeout_ms;std::this_thread::sleep_for(std::chrono::nanoseconds(remaining));
      return FrameWaitResult::Signaled;
#endif
    },report);
  }
};
}
