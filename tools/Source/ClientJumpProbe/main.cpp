#include "LocalSession.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <thread>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace octaryn::client::app;
using Clock = std::chrono::steady_clock;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
double seconds(Clock::duration value) { return std::chrono::duration<double>(value).count(); }
class SampleWait {
public:
#if defined(_WIN32)
 SampleWait() : timer_(CreateWaitableTimerExW(nullptr, nullptr,
 CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)) {
 require(timer_ != nullptr, "create high-resolution sample timer");
 }
 ~SampleWait() { CloseHandle(timer_); }
 void wait() {
 LARGE_INTEGER due{};
 due.QuadPart = -20000;
 require(SetWaitableTimerEx(timer_, &due, 0, nullptr, nullptr, nullptr, 0) != FALSE,
 "arm sample timer");
 require(WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0, "wait sample timer");
 }
private:
 HANDLE timer_{};
#else
 void wait() { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
#endif
};
} // namespace

// Measures presented flight from domain input onset through the real transport.
int main(int argc, char** argv) {
 try {
 require(argc >= 4, "arguments: canonical bundle, fresh isolated world, logs [host:port] [--max-takeoff-ms value]");
 const char* endpoint = nullptr;
 double max_takeoff_seconds = 0;
 for (int index = 4; index < argc; ++index) {
  if (std::strcmp(argv[index], "--max-takeoff-ms") == 0) {
   require(++index < argc, "missing takeoff budget");
   char* end = nullptr;
   const double value = std::strtod(argv[index], &end);
   require(end != argv[index] && *end == '\0' && std::isfinite(value) && value > 0 && value <= 1000,
       "takeoff budget must be greater than zero and at most 1000 milliseconds");
   max_takeoff_seconds = value / 1000;
  } else {
   require(!endpoint && argv[index][0] != '-', "unexpected jump probe argument");
   endpoint = argv[index];
  }
 }
 require(!std::filesystem::exists(argv[2]), "probe requires a fresh isolated world");
 LocalSession session;
 if (endpoint) {
 if (!session.start_remote(argv[1], argv[2], 2, endpoint, argv[3])) {
 std::fprintf(stderr, "start remote authority failed: %s\n", session.status().c_str());
 return 1;
 }
 }
 else require(session.start(argv[1], argv[2], 2, argv[3]), "start packaged authority");
 LocalPlayerInput input{};
 LocalPlayerPose pose{};
 SampleWait sample_wait;
 auto previous = Clock::now();
    const auto startup = previous;
    bool track_movement = false;
uint64_t held_samples = 0, movement_samples = 0, initial_underruns = 0;
 uint64_t max_pending = 0;
    double held_seconds = 0, current_hold_seconds = 0, max_hold_seconds = 0;
 auto update = [&] {
 const auto now = Clock::now();
 const double dt = seconds(now - previous);
 session.update(input, dt);
 previous = now;
 require(session.running(), "authority exited");
      const bool ready = session.player_pose(pose);
      if (track_movement && ready) {
const auto stats = session.movement_stats();
 max_pending = std::max(max_pending, uint64_t(stats.pending));
        ++movement_samples;
        if (stats.holding) {
          ++held_samples;
          held_seconds += dt;
          current_hold_seconds += dt;
          max_hold_seconds = std::max(max_hold_seconds, current_hold_seconds);
        } else current_hold_seconds = 0;
      }
 sample_wait.wait();
 return ready;
 };
 while (!update()) require(seconds(Clock::now() - startup) < 30, "authority startup timeout");
 auto settle = Clock::now();
 while (seconds(Clock::now() - settle) < 1) update();
 require(!pose.flying, "player must start in walk mode");
    require(pose.on_ground, "player not grounded before jump");
    initial_underruns = session.movement_stats().underruns;
    track_movement = true;

 auto jump_once = [&](int index, double hold_seconds, bool forward) {
 require(pose.on_ground, "player not grounded before jump");
 const float ground_y = pose.y;
 const float start_x = pose.x, start_z = pose.z;
 input.forward = forward;
 // Alternate headings to keep repeated forward cases near the spawn surface.
 input.yaw = forward && index % 2 == 0 ? 3.14159265f : 0.0f;
 input.up = true;
 const auto onset = Clock::now();
 float apex = ground_y;
 float previous_y = ground_y;
 float max_vertical_step = 0;
 bool left_ground = false;
 bool landed = false;
 double leave_seconds = 0;
 double air_seconds = 0;
 double released_seconds = 0;
 double max_sample_gap = 0;
 auto last_sample = onset;
 unsigned samples = 0;
 while (seconds(Clock::now() - onset) < 2.5) {
 const double elapsed_before = seconds(Clock::now() - onset);
 if (input.up && elapsed_before >= hold_seconds) {
 input.up = false;
 released_seconds = elapsed_before;
 }
 update();
 const auto sampled = Clock::now();
 const double elapsed = seconds(sampled - onset);
 max_sample_gap = std::max(max_sample_gap, seconds(sampled - last_sample));
 last_sample = sampled;
 ++samples;
 apex = std::max(apex, pose.y);
 max_vertical_step = std::max(max_vertical_step, std::abs(pose.y - previous_y));
 previous_y = pose.y;
 if (!pose.on_ground && !left_ground) {
 left_ground = true;
 leave_seconds = elapsed;
 }
 if (landed) require(pose.on_ground, "held jump unexpectedly repeated after landing");
 if (left_ground && pose.on_ground && !landed) {
 air_seconds = elapsed - leave_seconds;
 landed = true;
 }
 if (landed && !input.up) break;
 }
 input.up = false;
 input.forward = false;
 const float apex_delta = apex - ground_y;
 const float horizontal = std::hypot(pose.x - start_x, pose.z - start_z);
 std::printf("jump_%d mode=%s hold_seconds=%.4f released_seconds=%.4f left_ground=%d landed=%d "
 "onset_to_takeoff=%.4f apex_delta=%.4f air_seconds=%.4f horizontal=%.4f "
 "ground_y=%.4f landed_y=%.4f samples=%u max_sample_gap=%.4f max_vertical_step=%.4f\n",
 index, forward ? "forward" : "standing", hold_seconds, released_seconds,
 left_ground, landed, leave_seconds, apex_delta, air_seconds, horizontal,
 ground_y, pose.y, samples, max_sample_gap, max_vertical_step);
 std::fflush(stdout);
 require(left_ground, "jump never left the ground");
 require(max_takeoff_seconds == 0 || leave_seconds <= max_takeoff_seconds,
     "local takeoff exceeded response budget");
 require(landed, "jump never landed");
 require(apex_delta > 0.6f && apex_delta < 1.8f, "jump apex outside expected range");
 require(air_seconds > 0.3 && air_seconds < 1.5, "jump air time outside expected range");
 if (!forward) require(std::abs(pose.y - ground_y) < 0.25f, "standing landing height drifted");
 if (forward) require(horizontal > 0.5f, "forward jump lost horizontal motion");
 const auto pause = Clock::now();
 while (seconds(Clock::now() - pause) < 0.5) {
 update();
      require(pose.on_ground || pose.velocity_y <= 0.1f,
          "released jump unexpectedly repeated upward takeoff");
 }
 };

 int index = 0;
 int failures = 0;
 auto run_case = [&](double hold, bool forward) {
 try { jump_once(++index, hold, forward); }
 catch (const std::exception& error) {
 ++failures;
 std::printf("jump_%d failed=%s\n", index, error.what());
 input.up = false;
 input.forward = false;
 const auto pause = Clock::now();
 while (seconds(Clock::now() - pause) < 0.5) update();
 }
 };
 for (const bool forward : {false, true}) {
 run_case(0.15, forward);
 for (int repeat = 0; repeat < 3; ++repeat) run_case(0.02, forward);
 }
 run_case(1.1, false);
 run_case(0.02, false);
    std::printf("jump_cases=%d failures=%d\n", index, failures);
    std::printf("jump_playback samples=%llu held_samples=%llu held_seconds=%.4f max_hold_seconds=%.4f underruns=%llu\n",
        static_cast<unsigned long long>(movement_samples), static_cast<unsigned long long>(held_samples),
        held_seconds, max_hold_seconds,
        static_cast<unsigned long long>(session.movement_stats().underruns - initial_underruns));
 const auto prediction = session.movement_stats();
 std::printf("jump_prediction ack=%llu pending=%llu max_pending=%llu replays=%llu corrections=%llu overflows=%llu\n",
     static_cast<unsigned long long>(prediction.ack),
     static_cast<unsigned long long>(prediction.pending),
     static_cast<unsigned long long>(max_pending),
     static_cast<unsigned long long>(prediction.replays),
     static_cast<unsigned long long>(prediction.corrections),
     static_cast<unsigned long long>(prediction.overflows));
 require(prediction.overflows == 0, "prediction history overflowed");
 std::printf("jump_correction latest=%.6f maximum=%.6f\n",
     prediction.correction_distance, prediction.max_correction_distance);
 require(prediction.ack > 0, "server never acknowledged predicted input");
 require(failures == 0, "jump qualification cases failed");
 std::printf("jump_session=passed\n");
 session.stop();
 return 0;
 } catch (const std::exception& error) {
 std::fprintf(stderr, "jump_session=failed %s\n", error.what());
 return 1;
 }
}
