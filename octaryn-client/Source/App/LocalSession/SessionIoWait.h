#pragma once
#include <chrono>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <condition_variable>
#include <mutex>
#endif

namespace octaryn::client::app::local_session {
class SessionIoWait {
public:
  using Clock = std::chrono::steady_clock;
  SessionIoWait() {
#if defined(_WIN32)
    stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!stop_ || !timer_) {
      if (stop_) CloseHandle(stop_);
      if (timer_) CloseHandle(timer_);
      throw std::runtime_error("Session I/O timer/event creation failed");
    }
#endif
  }
  ~SessionIoWait() {
#if defined(_WIN32)
    CloseHandle(timer_);CloseHandle(stop_);
#endif
  }
  SessionIoWait(const SessionIoWait&) = delete;
  SessionIoWait& operator=(const SessionIoWait&) = delete;
  bool until(Clock::time_point deadline) {
#if defined(_WIN32)
    if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return false;
    const auto remaining = deadline - Clock::now();
    if (remaining <= Clock::duration::zero()) return true;
    using TimerUnit = std::chrono::duration<long long, std::ratio<1, 10000000>>;
    LARGE_INTEGER due{};
    due.QuadPart = -std::chrono::ceil<TimerUnit>(remaining).count();
    if (!SetWaitableTimerEx(timer_, &due, 0, nullptr, nullptr, nullptr, 0))
      throw std::runtime_error("Session I/O timer arm failed");
    HANDLE handles[]{stop_, timer_};
    const DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    if (result == WAIT_OBJECT_0) return false;
    if (result == WAIT_OBJECT_0 + 1) return true;
    throw std::runtime_error("Session I/O timer wait failed");
#else
    std::unique_lock lock(mutex_);
    return !wake_.wait_until(lock, deadline, [&] { return stopped_; });
#endif
  }
  void stop() {
#if defined(_WIN32)
    SetEvent(stop_);
#else
    { std::lock_guard lock(mutex_); stopped_ = true; }
    wake_.notify_all();
#endif
  }
  static Clock::time_point next(Clock::time_point previous, Clock::time_point now) {
    constexpr auto interval = std::chrono::microseconds(16667);
    auto deadline = previous + interval;
    if (deadline <= now) deadline += interval * ((now - deadline) / interval + 1);
    return deadline;
  }
private:
#if defined(_WIN32)
  HANDLE stop_{}, timer_{};
#else
  std::mutex mutex_;
  std::condition_variable wake_;
  bool stopped_{};
#endif
};
}
