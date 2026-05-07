#include "HostPolicy.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

bool expect_equal(std::string_view label, uint32_t actual, uint32_t expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %u, got %u\n",
               static_cast<int>(label.size()), label.data(), expected, actual);
  return false;
}

bool expect_equal(std::string_view label, int32_t actual, int32_t expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %d, got %d\n",
               static_cast<int>(label.size()), label.data(), expected, actual);
  return false;
}

bool expect_equal(std::string_view label, double actual, double expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %.9f, got %.9f\n",
               static_cast<int>(label.size()), label.data(), expected, actual);
  return false;
}

bool expect_equal(std::string_view label, const char *actual,
                  std::string_view expected) {
  if (actual != nullptr && std::string_view(actual) == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %.*s, got %s\n",
               static_cast<int>(label.size()), label.data(),
               static_cast<int>(expected.size()), expected.data(),
               actual == nullptr ? "<null>" : actual);
  return false;
}

bool expect_null(std::string_view label, const char *actual) {
  if (actual == nullptr) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected <null>, got %s\n",
               static_cast<int>(label.size()), label.data(), actual);
  return false;
}

void set_environment_value(const char *name, const char *value) {
#if defined(_WIN32)
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void clear_environment_value(const char *name) {
#if defined(_WIN32)
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

bool validate_environment_flags() {
  bool ok = true;
  set_environment_value("OCTARYN_SERVER_DISABLE_GAME_MODULES", "yes");
  clear_environment_value("OCTARYN_CLIENT_DISABLE_GAME_MODULES");
  clear_environment_value("OCTARYN_SERVER_PROCESS_STREAM_LIVE");
  auto policy = octaryn_server_host_get_startup_policy();
  ok &= expect_equal("server disable modules", policy.disable_game_modules, 1u);
  ok &= expect_equal("server live stream disabled", policy.live_process_stream,
                     0u);
  ok &= expect_equal("server live stream interval",
                     policy.live_stream_interval_ms, 50u);

  clear_environment_value("OCTARYN_SERVER_DISABLE_GAME_MODULES");
  set_environment_value("OCTARYN_CLIENT_DISABLE_GAME_MODULES", "TRUE");
  set_environment_value("OCTARYN_SERVER_PROCESS_STREAM_LIVE", "1");
  policy = octaryn_server_host_get_startup_policy();
  ok &= expect_equal("client disable modules", policy.disable_game_modules, 1u);
  ok &= expect_equal("server live stream enabled", policy.live_process_stream,
                     1u);

  set_environment_value("OCTARYN_SERVER_PROCESS_STREAM_LIVE", "0");
  ok &= expect_equal("explicit disabled env",
                     octaryn_server_host_environment_enabled(
                         "OCTARYN_SERVER_PROCESS_STREAM_LIVE"),
                     0u);
  set_environment_value("OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY", "TrUe");
  ok &= expect_equal("metadata-only env",
                     octaryn_server_host_environment_enabled(
                         "OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY"),
                     1u);
  set_environment_value("OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY", "yes");
  ok &= expect_equal("yes env",
                     octaryn_server_host_environment_enabled(
                         "OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY"),
                     1u);
  set_environment_value("OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY", "false");
  ok &= expect_equal("false env",
                     octaryn_server_host_environment_enabled(
                         "OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY"),
                     0u);
  ok &= expect_equal("null env name",
                     octaryn_server_host_environment_enabled(nullptr), 0u);
  ok &= expect_equal("empty env name",
                     octaryn_server_host_environment_enabled(""), 0u);

  clear_environment_value("OCTARYN_SERVER_DISABLE_GAME_MODULES");
  clear_environment_value("OCTARYN_CLIENT_DISABLE_GAME_MODULES");
  clear_environment_value("OCTARYN_SERVER_PROCESS_STREAM_LIVE");
  clear_environment_value("OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY");
  return ok;
}

bool validate_live_stream_paths() {
  clear_environment_value("OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH");
  clear_environment_value("OCTARYN_SERVER_CHUNK_STREAM_PATH");
  clear_environment_value("OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH");
  clear_environment_value("OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH");
  clear_environment_value("OCTARYN_SERVER_WORLD_TIME_INTENT_PATH");
  auto paths = octaryn_server_host_get_live_stream_paths();
  bool ok = true;
  ok &= expect_null("missing chunk view path", paths.chunk_view_intent_path);
  ok &= expect_null("missing chunk stream path", paths.chunk_stream_path);
  ok &= expect_null("missing player input path", paths.player_input_intent_path);
  ok &= expect_null("missing block interaction path",
                    paths.block_interaction_intent_path);
  ok &= expect_null("missing world time path", paths.world_time_intent_path);
  ok &= expect_equal("missing metadata-only flag", paths.metadata_only, 0u);

  set_environment_value("OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH",
                        "/tmp/octaryn/chunk_view_intent.json");
  set_environment_value("OCTARYN_SERVER_CHUNK_STREAM_PATH",
                        "/tmp/octaryn/chunk_stream.json");
  set_environment_value("OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH",
                        "/tmp/octaryn/player_input_intent.json");
  set_environment_value("OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH",
                        "/tmp/octaryn/block_interaction_intent.json");
  set_environment_value("OCTARYN_SERVER_WORLD_TIME_INTENT_PATH",
                        "/tmp/octaryn/world_time_intent.json");
  set_environment_value("OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY", "TrUe");
  paths = octaryn_server_host_get_live_stream_paths();
  ok &= expect_equal("chunk view path", paths.chunk_view_intent_path,
                     "/tmp/octaryn/chunk_view_intent.json");
  ok &= expect_equal("chunk stream path", paths.chunk_stream_path,
                     "/tmp/octaryn/chunk_stream.json");
  ok &= expect_equal("player input path", paths.player_input_intent_path,
                     "/tmp/octaryn/player_input_intent.json");
  ok &= expect_equal("block interaction path",
                     paths.block_interaction_intent_path,
                     "/tmp/octaryn/block_interaction_intent.json");
  ok &= expect_equal("world time path", paths.world_time_intent_path,
                     "/tmp/octaryn/world_time_intent.json");
  ok &= expect_equal("metadata-only flag", paths.metadata_only, 1u);

  clear_environment_value("OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH");
  clear_environment_value("OCTARYN_SERVER_CHUNK_STREAM_PATH");
  clear_environment_value("OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH");
  clear_environment_value("OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH");
  clear_environment_value("OCTARYN_SERVER_WORLD_TIME_INTENT_PATH");
  clear_environment_value("OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY");
  return ok;
}

bool validate_live_stream_request_plan() {
  bool ok = true;
  auto paths = octaryn_server_host_live_stream_paths{
      .chunk_view_intent_path = nullptr,
      .chunk_stream_path = "/tmp/octaryn/chunk_stream.json",
      .player_input_intent_path = nullptr,
      .block_interaction_intent_path = nullptr,
      .world_time_intent_path = nullptr,
      .metadata_only = 0u,
  };

  auto plan = octaryn_server_host_plan_live_stream_request(&paths);
  ok &= expect_equal("missing chunk view should handle", plan.should_handle,
                     0u);
  ok &= expect_equal("missing chunk view should continue", plan.should_continue,
                     0u);
  ok &= expect_equal("missing chunk view result", plan.handle_result, 0);
  ok &= expect_equal("missing chunk view reason",
                     octaryn_server_host_live_stream_request_reason_name(
                         plan.reason),
                     "missing_chunk_view_intent");

  paths.chunk_view_intent_path = "   ";
  plan = octaryn_server_host_plan_live_stream_request(&paths);
  ok &= expect_equal("blank chunk view should handle", plan.should_handle, 0u);
  ok &= expect_equal("blank chunk view result", plan.handle_result, 0);

  paths.chunk_view_intent_path = "/tmp/octaryn/chunk_view_intent.json";
  paths.chunk_stream_path = nullptr;
  plan = octaryn_server_host_plan_live_stream_request(&paths);
  ok &= expect_equal("missing stream should handle", plan.should_handle, 1u);
  ok &= expect_equal("missing stream should continue", plan.should_continue,
                     0u);
  ok &= expect_equal("missing stream result", plan.handle_result, -1);
  ok &= expect_equal("missing stream reason",
                     octaryn_server_host_live_stream_request_reason_name(
                         plan.reason),
                     "missing_stream_path");

  paths.chunk_stream_path = " ";
  plan = octaryn_server_host_plan_live_stream_request(&paths);
  ok &= expect_equal("blank stream should handle", plan.should_handle, 1u);
  ok &= expect_equal("blank stream result", plan.handle_result, -1);

  paths.chunk_stream_path = "/tmp/octaryn/chunk_stream.json";
  plan = octaryn_server_host_plan_live_stream_request(&paths);
  ok &= expect_equal("ready request should handle", plan.should_handle, 1u);
  ok &= expect_equal("ready request should continue", plan.should_continue,
                     1u);
  ok &= expect_equal("ready request result", plan.handle_result, 0);
  ok &= expect_equal("ready request reason",
                     octaryn_server_host_live_stream_request_reason_name(
                         plan.reason),
                     "none");

  ok &= expect_equal("unknown request reason",
                     octaryn_server_host_live_stream_request_reason_name(99u),
                     "none");
  return ok;
}

bool validate_startup_frame() {
  octaryn_host_frame_snapshot frame = {};
  octaryn_server_host_create_startup_frame(&frame);
  bool ok = true;
  ok &= expect_equal("startup frame version", frame.version, 1u);
  ok &= expect_equal("startup frame size", frame.size,
                     OCTARYN_HOST_FRAME_SNAPSHOT_SIZE);
  ok &= expect_equal("startup input version", frame.input.version, 1u);
  ok &= expect_equal("startup input size", frame.input.size,
                     OCTARYN_HOST_INPUT_SNAPSHOT_SIZE);
  ok &= expect_equal("startup timing version", frame.timing.version, 1u);
  ok &= expect_equal("startup timing size", frame.timing.size,
                     OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE);
  ok &= expect_equal("startup frame index",
                     static_cast<uint32_t>(frame.timing.frame_index), 0u);
  ok &= expect_equal("startup delta seconds", frame.timing.delta_seconds,
                     1.0 / 60.0);
  return ok;
}

struct LiveStreamLoopState {
  uint32_t iteration_count = 0;
  uint32_t stop_after = 0;
  int32_t stop_result = 0;
};

int32_t live_stream_iteration(void *context) {
  if (context == nullptr) {
    return -1;
  }

  auto *state = static_cast<LiveStreamLoopState *>(context);
  ++state->iteration_count;
  return state->iteration_count >= state->stop_after ? state->stop_result : 0;
}

bool validate_live_stream_loop() {
  LiveStreamLoopState state{
      .iteration_count = 0,
      .stop_after = 3,
      .stop_result = 7,
  };
  bool ok = true;
  const int32_t result =
      octaryn_server_host_run_live_stream_loop(0u, live_stream_iteration, &state);
  ok &= expect_equal("live stream loop result", result, 7);
  ok &= expect_equal("live stream loop iterations", state.iteration_count, 3u);
  ok &= expect_equal("live stream loop missing callback",
                     octaryn_server_host_run_live_stream_loop(0u, nullptr,
                                                              &state),
                     -1);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_environment_flags();
  ok &= validate_live_stream_paths();
  ok &= validate_live_stream_request_plan();
  ok &= validate_startup_frame();
  ok &= validate_live_stream_loop();
  return ok ? 0 : 1;
}
