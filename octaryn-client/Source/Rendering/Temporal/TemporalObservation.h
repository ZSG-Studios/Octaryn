#pragma once
#include <chrono>

namespace octaryn::client::rendering {
// Explicit diagnostic observation must not age otherwise valid frame history.
class TemporalObservation {
public:
  using Clock=std::chrono::steady_clock;
  TemporalObservation(Clock::time_point& last,bool active,Clock::time_point start=Clock::now())
      :last_(last),start_(start),active_(active) {}
  TemporalObservation(const TemporalObservation&)=delete;
  TemporalObservation& operator=(const TemporalObservation&)=delete;
  ~TemporalObservation() {finish();}
  double elapsed_ms(Clock::time_point end=Clock::now()) const {
    return std::chrono::duration<double,std::milli>(end-start_).count();
  }
  void finish(Clock::time_point end=Clock::now()) {
    if(active_ && last_.time_since_epoch().count()!=0 && end>=start_)last_+=end-start_;
    active_=false;
  }
private:
  Clock::time_point& last_;
  Clock::time_point start_;
  bool active_;
};
}
