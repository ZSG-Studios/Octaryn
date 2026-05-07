#include "ChunkColumnStream.h"

#include <cstdint>
#include <cstdio>
#include <string_view>

namespace {

bool expect_equal(std::string_view label, auto actual, auto expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

struct ProcessTickProbeState {
  uint32_t host_only_count = 0;
  uint32_t tick_count = 0;
  uint64_t last_frame_index = 0;
  int32_t result = 0;
};

int32_t host_only_process_tick(void *context,
                               const octaryn_host_frame_snapshot *frame) {
  auto *state = static_cast<ProcessTickProbeState *>(context);
  if (state == nullptr || frame == nullptr) {
    return -1;
  }

  ++state->host_only_count;
  state->last_frame_index = frame->timing.frame_index;
  return state->result;
}

int32_t full_process_tick(void *context,
                          const octaryn_host_frame_snapshot *frame) {
  auto *state = static_cast<ProcessTickProbeState *>(context);
  if (state == nullptr || frame == nullptr) {
    return -1;
  }

  ++state->tick_count;
  state->last_frame_index = frame->timing.frame_index;
  return state->result;
}

} // namespace

bool validate_chunk_stream_process_tick() {
  bool ok = true;
  ProcessTickProbeState tick_state{};
  const auto no_tick = octaryn_server_chunk_stream_process_tick_decision{};
  ok &= expect_equal(
      "process tick no-op execute",
      octaryn_server_chunk_stream_execute_process_tick(
          &no_tick, nullptr, nullptr, nullptr, &tick_state),
      0);
  ok &= expect_equal("process tick no-op count", tick_state.host_only_count,
                     0u);

  const auto metadata_tick =
      octaryn_server_chunk_stream_decide_process_tick(0u, 1u, 1u);
  ok &= expect_equal(
      "process tick default host-only execute",
      octaryn_server_chunk_stream_execute_process_tick(
          &metadata_tick, nullptr, host_only_process_tick, full_process_tick,
          &tick_state),
      0);
  ok &= expect_equal("process tick host-only count", tick_state.host_only_count,
                     1u);
  ok &= expect_equal("process tick default frame index",
                     tick_state.last_frame_index, uint64_t{1u});

  auto player_tick =
      octaryn_server_chunk_stream_decide_process_tick(1u, 0u, 0u);
  octaryn_host_frame_snapshot frame{};
  frame.timing.frame_index = 42u;
  ok &= expect_equal(
      "process tick player execute",
      octaryn_server_chunk_stream_execute_process_tick(
          &player_tick, &frame, host_only_process_tick, full_process_tick,
          &tick_state),
      0);
  ok &= expect_equal("process tick full count", tick_state.tick_count, 1u);
  ok &= expect_equal("process tick supplied frame index",
                     tick_state.last_frame_index, uint64_t{42u});

  ok &= expect_equal(
      "process tick missing callback",
      octaryn_server_chunk_stream_execute_process_tick(&player_tick, &frame,
                                                       nullptr, nullptr,
                                                       &tick_state),
      -1);
  tick_state.result = -7;
  ok &= expect_equal(
      "process tick callback result",
      octaryn_server_chunk_stream_execute_process_tick(
          &player_tick, &frame, host_only_process_tick, full_process_tick,
          &tick_state),
      -7);
  return ok;
}
