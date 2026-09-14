#pragma once
#include <chrono>
#include <cstdint>
namespace octaryn::client::rendering {
// Per-frame aggregate, emitted/reset by the existing opt-in GPU CSV only.
struct WorldMeshTimings {
  double halo_decode{},allocation{},upload{},encoding{},submission{},readback{},fence_wait{},release{},publication{};
  std::uint64_t jobs_started{},count_submits{},emit_submits{},halo_published{},halo_discarded{};
};
class WorldMeshTimer {
  using Clock=std::chrono::steady_clock;
  double* total_;
  Clock::time_point start_;
public:
  explicit WorldMeshTimer(double* total):total_(total),start_(total?Clock::now():Clock::time_point{}) {}
  ~WorldMeshTimer() {if(total_) *total_+=std::chrono::duration<double,std::milli>(Clock::now()-start_).count();}
};
}
