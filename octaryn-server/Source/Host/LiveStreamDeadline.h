#pragma once
#include <chrono>

namespace octaryn::server::host {
class LiveStreamDeadline {
public:
  using Clock = std::chrono::steady_clock;
  LiveStreamDeadline(Clock::duration interval, Clock::time_point start)
      : interval_(interval), deadline_(start) {}

  Clock::time_point next(Clock::time_point now) {
    if (interval_ <= Clock::duration::zero()) return now;
    deadline_ += interval_;
    // Work or a late wake may miss several slots. Wait for the next future slot;
    // never run a burst of stale input ticks to catch up.
    if (deadline_ <= now) deadline_ += interval_ * ((now - deadline_) / interval_ + 1);
    return deadline_;
  }
private:
  Clock::duration interval_;
  Clock::time_point deadline_;
};
}
