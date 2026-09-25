#include "HostPolicy.h"
#include "LiveStreamDeadline.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr uint32_t default_live_stream_interval_ms = 1;
constexpr double startup_delta_seconds = 1.0 / 60.0;
constexpr const char *chunk_view_intent_path_env =
    "OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH";
constexpr const char *chunk_stream_path_env = "OCTARYN_SERVER_CHUNK_STREAM_PATH";
constexpr const char *player_input_intent_path_env =
    "OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH";
constexpr const char *player_state_stream_path_env =
    "OCTARYN_SERVER_PLAYER_STATE_STREAM_PATH";
constexpr const char *block_interaction_intent_path_env =
    "OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH";
constexpr const char *world_time_intent_path_env =
    "OCTARYN_SERVER_WORLD_TIME_INTENT_PATH";
constexpr const char *chunk_stream_metadata_only_env =
    "OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY";
constexpr const char *live_stream_interval_ms_env =
    "OCTARYN_SERVER_PROCESS_STREAM_INTERVAL_MS";

enum live_stream_request_reason : uint32_t {
  live_stream_request_reason_none = 0u,
  live_stream_request_reason_missing_chunk_view_intent = 1u,
  live_stream_request_reason_missing_stream_path = 2u,
};

octaryn_server_host_live_stream_request_plan stop_live_stream_request(
    uint32_t reason, int32_t result) {
  return octaryn_server_host_live_stream_request_plan{
      .should_handle = reason != live_stream_request_reason_missing_chunk_view_intent
                           ? 1u
                           : 0u,
      .should_continue = 0u,
      .handle_result = result,
      .reason = reason,
  };
}

bool equals_ignore_case(const char *value, const char *expected) {
  if (value == nullptr || expected == nullptr) {
    return false;
  }

  while (*value != '\0' && *expected != '\0') {
    const auto left =
        static_cast<char>(std::tolower(static_cast<unsigned char>(*value)));
    const auto right =
        static_cast<char>(std::tolower(static_cast<unsigned char>(*expected)));
    if (left != right) {
      return false;
    }

    ++value;
    ++expected;
  }

  return *value == '\0' && *expected == '\0';
}

bool enabled_value(const char *value) {
  return value != nullptr && value[0] != '\0' &&
         (std::strcmp(value, "1") == 0 || equals_ignore_case(value, "true") ||
          equals_ignore_case(value, "yes"));
}

bool environment_enabled(const char *name) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }

  return enabled_value(std::getenv(name));
}

uint32_t nonnegative_environment_uint(const char *name, uint32_t fallback) {
  const char *value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return fallback;
  }

  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (end == value || parsed > std::numeric_limits<uint32_t>::max()) {
    return fallback;
  }

  return static_cast<uint32_t>(parsed);
}

bool has_path_text(const char *value) {
  if (value == nullptr) {
    return false;
  }

  while (*value != '\0') {
    if (std::isspace(static_cast<unsigned char>(*value)) == 0) {
      return true;
    }

    ++value;
  }

  return false;
}

const char *live_stream_request_reason_name(uint32_t reason) {
  switch (reason) {
  case live_stream_request_reason_missing_chunk_view_intent:
    return "missing_chunk_view_intent";
  case live_stream_request_reason_missing_stream_path:
    return "missing_stream_path";
  case live_stream_request_reason_none:
  default:
    return "none";
  }
}

class LiveStreamWait {
public:
  explicit LiveStreamWait(bool enabled) {
#if defined(_WIN32)
    if (enabled) timer_ = CreateWaitableTimerExW(nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
    valid_ = !enabled || timer_ != nullptr;
#else
    (void)enabled;
#endif
  }
  ~LiveStreamWait() {
#if defined(_WIN32)
    if (timer_) CloseHandle(timer_);
#endif
  }
  LiveStreamWait(const LiveStreamWait&) = delete;
  LiveStreamWait& operator=(const LiveStreamWait&) = delete;
  bool valid() const { return valid_; }
  bool until(std::chrono::steady_clock::time_point deadline) {
#if defined(_WIN32)
    const auto remaining = deadline - std::chrono::steady_clock::now();
    if (remaining <= std::chrono::steady_clock::duration::zero()) return true;
    using TimerUnit = std::chrono::duration<long long, std::ratio<1, 10000000>>;
    LARGE_INTEGER due{};
    due.QuadPart = -std::chrono::ceil<TimerUnit>(remaining).count();
    return timer_ && SetWaitableTimerEx(timer_, &due, 0, nullptr, nullptr, nullptr, 0) &&
        WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0;
#else
    std::this_thread::sleep_until(deadline);
    return true;
#endif
  }
private:
  bool valid_{true};
#if defined(_WIN32)
  HANDLE timer_{};
#endif
};

} // namespace

extern "C" {

uint32_t octaryn_server_host_environment_enabled(const char *name) {
  return environment_enabled(name) ? 1u : 0u;
}

octaryn_server_host_startup_policy octaryn_server_host_get_startup_policy() {
  return {
      environment_enabled("OCTARYN_SERVER_PROCESS_STREAM_LIVE") ? 1u : 0u,
      nonnegative_environment_uint(live_stream_interval_ms_env,
                                   default_live_stream_interval_ms),
  };
}

octaryn_server_host_live_stream_paths
octaryn_server_host_get_live_stream_paths() {
  return {
      std::getenv(chunk_view_intent_path_env),
      std::getenv(chunk_stream_path_env),
      std::getenv(player_input_intent_path_env),
      std::getenv(player_state_stream_path_env),
      std::getenv(block_interaction_intent_path_env),
      std::getenv(world_time_intent_path_env),
      environment_enabled(chunk_stream_metadata_only_env) ? 1u : 0u,
  };
}

octaryn_server_host_live_stream_request_plan
octaryn_server_host_plan_live_stream_request(
    const octaryn_server_host_live_stream_paths *paths) {
  if (paths == nullptr || !has_path_text(paths->chunk_view_intent_path)) {
    return stop_live_stream_request(
        live_stream_request_reason_missing_chunk_view_intent, 0);
  }

  if (!has_path_text(paths->chunk_stream_path)) {
    return stop_live_stream_request(
        live_stream_request_reason_missing_stream_path, -1);
  }

  return octaryn_server_host_live_stream_request_plan{
      .should_handle = 1u,
      .should_continue = 1u,
      .handle_result = 0,
      .reason = live_stream_request_reason_none,
  };
}

const char *octaryn_server_host_live_stream_request_reason_name(
    uint32_t reason) {
  return live_stream_request_reason_name(reason);
}

void octaryn_server_host_create_startup_frame(
    octaryn_host_frame_snapshot *frame) {
  if (frame == nullptr) {
    return;
  }

  *frame = {};
  frame->version = 1u;
  frame->size = OCTARYN_HOST_FRAME_SNAPSHOT_SIZE;
  frame->input.version = 1u;
  frame->input.size = OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
  frame->timing.version = 1u;
  frame->timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
  frame->timing.frame_index = 0u;
  frame->timing.delta_seconds = startup_delta_seconds;
}

int32_t octaryn_server_host_run_live_stream_loop(
    uint32_t interval_ms, octaryn_server_host_live_stream_iteration_fn iteration,
    void *context) {
  if (iteration == nullptr) {
    return -1;
  }

  LiveStreamWait wait(interval_ms != 0);
  if (!wait.valid()) {
    std::fprintf(stderr, "server_live_timer failed=1 reason=create\n");
    return -2;
  }
  using octaryn::server::host::LiveStreamDeadline;
  LiveStreamDeadline deadline(std::chrono::milliseconds(interval_ms),
      LiveStreamDeadline::Clock::now());

  while (true) {
    const int32_t result = iteration(context);
    if (result != 0) {
      return result;
    }

    if (interval_ms != 0 && !wait.until(deadline.next(LiveStreamDeadline::Clock::now()))) {
      std::fprintf(stderr, "server_live_timer failed=1 reason=wait\n");
      return -2;
    }
  }
}
}
