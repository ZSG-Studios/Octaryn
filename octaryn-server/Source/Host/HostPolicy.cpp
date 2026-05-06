#include "HostPolicy.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

constexpr uint32_t live_stream_interval_ms = 50;
constexpr double startup_delta_seconds = 1.0 / 60.0;

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

void sleep_live_stream_interval(uint32_t interval_ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
}

} // namespace

extern "C" {

uint32_t octaryn_server_host_environment_enabled(const char *name) {
  return environment_enabled(name) ? 1u : 0u;
}

octaryn_server_host_startup_policy octaryn_server_host_get_startup_policy() {
  const bool disable_game_modules =
      environment_enabled("OCTARYN_SERVER_DISABLE_GAME_MODULES") ||
      environment_enabled("OCTARYN_CLIENT_DISABLE_GAME_MODULES");
  return {
      disable_game_modules ? 1u : 0u,
      environment_enabled("OCTARYN_SERVER_PROCESS_STREAM_LIVE") ? 1u : 0u,
      live_stream_interval_ms,
  };
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

  while (true) {
    const int32_t result = iteration(context);
    if (result != 0) {
      return result;
    }

    sleep_live_stream_interval(interval_ms);
  }
}
}
