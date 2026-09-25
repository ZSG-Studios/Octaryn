#pragma once

#include <cmath>
#include <cstdint>

namespace octaryn::client::app {
class FramePacing {
public:
  void invalidate_display() { display_dirty_ = true; }

  template<class Query>
  void update_display(Query query) {
    if (!display_dirty_) return;
    const double rate = query();
    ++display_queries_;
    refresh_hz_ = std::isfinite(rate) && rate > 1.0 ? rate : 0.0;
    display_dirty_ = false;
  }

  unsigned display_queries() const { return display_queries_; }
  double refresh_hz() const { return refresh_hz_; }
  double target_hz(unsigned cap) const { return cap == 1 ? refresh_hz_ : double(cap); }

  std::uint64_t remaining_ns(std::uint64_t start, std::uint64_t now,
      unsigned cap, bool vsync, bool uncapped) const {
    if (uncapped || cap == 0) return 0;
    const double hz = cap == 1 ? refresh_hz_ : double(cap);
    if (hz <= 0.0) return 0;
    // FIFO already limits to the display rate. Lower explicit caps still apply.
    if (vsync && (cap == 1 || (refresh_hz_ > 0.0 && hz >= refresh_hz_))) return 0;
    const auto period = static_cast<std::uint64_t>(1'000'000'000.0 / hz);
    const auto work = now >= start ? now - start : 0;
    return work < period ? period - work : 0;
  }

private:
  double refresh_hz_{};
  bool display_dirty_ = true;
  unsigned display_queries_{};
};
}
