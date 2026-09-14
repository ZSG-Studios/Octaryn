#include "LocalSession.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace octaryn::client::app;
using Clock = std::chrono::steady_clock;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
double seconds(Clock::duration value) { return std::chrono::duration<double>(value).count(); }
double percentile(std::vector<double> values, double fraction) {
  if (values.empty()) return 0;
  std::sort(values.begin(), values.end());
  return values[static_cast<size_t>((values.size()-1)*fraction)];
}
}
int main(int argc, char** argv) {
  try {
    require(argc == 4, "arguments: canonical bundle, fresh isolated world, logs");
    require(!std::filesystem::exists(argv[2]), "probe requires a fresh isolated world");
    LocalSession session;
    require(session.start(argv[1], argv[2], 2, argv[3]), "start packaged authority");
    LocalPlayerInput input{};
    LocalPlayerPose pose{}, previous_pose{};
    auto previous = Clock::now();
    const auto startup = previous;
    auto update = [&] {
      const auto now = Clock::now();
      const double dt = seconds(now-previous);
      session.update(input, dt);
      previous = now;
      require(session.running(), "authority exited");
      const bool ready = session.player_pose(pose);
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      return ready;
    };
    while (!update()) require(seconds(Clock::now()-startup)<30, "authority startup timeout");
    // Domain-level input exercises the real session transport, never OS/UI input.
    input.flying = true; input.up = true; input.sprint = true;
    auto phase = Clock::now();
    while (seconds(Clock::now()-phase)<2) update();
    input.up = false; input.sprint = false; input.forward = true;
    phase = Clock::now();
    while (seconds(Clock::now()-phase)<1) update();
    require(pose.flying, "authority did not enter flight");
    std::vector<double> speeds, playback_rates, receive_intervals;
    unsigned holds{}, samples{};
    double distance{}, wall{}, source{};
    auto sampled = previous, tick_received = sampled;
    previous_pose = pose; phase = sampled;
    const auto initial_underruns = session.movement_stats().underruns;
    while (seconds(Clock::now()-phase)<5) {
      update();
      const auto now = previous;
      const double dt = seconds(now-sampled);
      const double ds = pose.source_seconds-previous_pose.source_seconds;
      const double dx = pose.x-previous_pose.x, dz = pose.z-previous_pose.z;
      require(ds >= 0, "presentation source time reversed");
      require(std::isfinite(pose.x) && std::isfinite(pose.y) && std::isfinite(pose.z), "nonfinite pose");
      if (ds <= 1e-9) ++holds;
      speeds.push_back(std::hypot(dx,dz)/dt);
      playback_rates.push_back(ds/dt);
      if (pose.source_tick != previous_pose.source_tick) {
        receive_intervals.push_back(seconds(now-tick_received)*1000);
        tick_received = now;
      }
      distance += std::hypot(dx,dz); wall += dt; source += ds; ++samples;
      previous_pose = pose; sampled = now;
    }
    const double velocity = std::hypot(pose.velocity_x,pose.velocity_z);
    require(distance>10 && velocity>0, "forward motion did not reach authority");
    require(std::abs(distance/source-velocity)<velocity*.1, "presentation distance disagrees with authoritative velocity");
    std::printf("movement_underruns=%llu final_buffer_ms=%.3f\n",
        static_cast<unsigned long long>(session.movement_stats().underruns-initial_underruns),
        session.movement_stats().buffered_seconds*1000);
    std::printf("movement_session=passed samples=%u holds=%u hold_percent=%.3f wall_seconds=%.6f source_seconds=%.6f distance=%.6f authoritative_speed=%.6f speed_p50=%.6f speed_p95=%.6f playback_p05=%.6f playback_p50=%.6f playback_p95=%.6f bracket_interval_p50_ms=%.3f bracket_interval_p95_ms=%.3f\n",
        samples, holds, 100.*holds/samples, wall, source, distance, velocity,
        percentile(speeds,.5),percentile(speeds,.95),percentile(playback_rates,.05),
        percentile(playback_rates,.5),percentile(playback_rates,.95),
        percentile(receive_intervals,.5),percentile(receive_intervals,.95));
    session.stop();
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr,"movement_session=failed %s\n",error.what()); return 1;
  }
}
