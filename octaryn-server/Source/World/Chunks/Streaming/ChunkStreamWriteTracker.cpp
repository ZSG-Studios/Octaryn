#include "ChunkColumnStream.h"

#include <cstdint>
#include <set>
#include <tuple>

namespace {

struct chunk_stream_window {
  int32_t center_chunk_x;
  int32_t center_chunk_z;
  uint32_t radius;

  [[nodiscard]] bool operator<(const chunk_stream_window &other) const {
    return std::tie(center_chunk_x, center_chunk_z, radius) <
           std::tie(other.center_chunk_x, other.center_chunk_z, other.radius);
  }

  [[nodiscard]] bool operator==(const chunk_stream_window &other) const {
    return center_chunk_x == other.center_chunk_x &&
           center_chunk_z == other.center_chunk_z && radius == other.radius;
  }
};

class chunk_stream_write_tracker {
public:
  [[nodiscard]] octaryn_server_chunk_stream_write_decision
  decide(bool metadata_only, bool submitted_block_commands,
         const chunk_stream_window &requested_window, bool has_previous_window,
         const chunk_stream_window &previous_window) const {
    const bool has_trusted_previous_window =
        written_windows_.contains(requested_window) ||
        (has_previous_window && written_windows_.contains(previous_window));
    const bool use_previous_window =
        metadata_only ? has_previous_window && has_trusted_previous_window
                      : has_previous_window;
    const bool should_skip = metadata_only && !submitted_block_commands &&
                             use_previous_window && has_last_written_ &&
                             last_written_ == requested_window &&
                             written_windows_.contains(requested_window);

    return octaryn_server_chunk_stream_write_decision{
        .use_previous_window = use_previous_window ? 1u : 0u,
        .should_write = should_skip ? 0u : 1u,
    };
  }

  void note_written(const chunk_stream_window &window) {
    has_last_written_ = true;
    last_written_ = window;
    written_windows_.insert(window);
  }

private:
  bool has_last_written_ = false;
  chunk_stream_window last_written_{};
  std::set<chunk_stream_window> written_windows_;
};

chunk_stream_write_tracker *as_tracker(void *tracker) {
  return static_cast<chunk_stream_write_tracker *>(tracker);
}

enum process_write_plan_reason : uint32_t {
  process_write_plan_reason_none = 0u,
  process_write_plan_reason_missing_intent = 1u,
  process_write_plan_reason_intent_read_retry = 2u,
  process_write_plan_reason_partial_intent = 3u,
  process_write_plan_reason_unsupported_intent = 4u,
  process_write_plan_reason_intent_read_failed = 5u,
  process_write_plan_reason_unchanged_window = 6u,
};

octaryn_server_chunk_stream_process_write_plan stop_plan(uint32_t reason,
                                                         int32_t result) {
  return octaryn_server_chunk_stream_process_write_plan{
      .should_continue = 0u,
      .should_write = 0u,
      .use_previous_window = 0u,
      .reason = reason,
      .handle_result = result,
      .center_chunk_x = 0,
      .center_chunk_z = 0,
      .radius = 0u,
  };
}

bool map_intent_read_result(
    int32_t intent_read_result, bool allow_transient_invalid,
    octaryn_server_chunk_stream_process_write_plan &plan) {
  if (intent_read_result == 0) {
    return true;
  }

  switch (intent_read_result) {
  case 1:
    plan = stop_plan(process_write_plan_reason_missing_intent,
                     allow_transient_invalid ? 0 : -1);
    return false;
  case -2:
    plan = stop_plan(process_write_plan_reason_intent_read_retry,
                     allow_transient_invalid ? 0 : -1);
    return false;
  case -3:
    plan = stop_plan(process_write_plan_reason_partial_intent,
                     allow_transient_invalid ? 0 : -1);
    return false;
  case -4:
    plan = stop_plan(process_write_plan_reason_unsupported_intent, -1);
    return false;
  default:
    plan = stop_plan(process_write_plan_reason_intent_read_failed, -1);
    return false;
  }
}

} // namespace

extern "C" {

void *octaryn_server_chunk_stream_write_tracker_create() {
  return new chunk_stream_write_tracker();
}

void octaryn_server_chunk_stream_write_tracker_destroy(void *tracker) {
  delete as_tracker(tracker);
}

octaryn_server_chunk_stream_write_decision
octaryn_server_chunk_stream_write_tracker_decide(
    void *tracker, uint32_t metadata_only, uint32_t submitted_block_commands,
    int32_t center_chunk_x, int32_t center_chunk_z, uint32_t radius,
    uint32_t has_previous_window, int32_t previous_center_chunk_x,
    int32_t previous_center_chunk_z, uint32_t previous_radius) {
  const auto *state = as_tracker(tracker);
  if (state == nullptr) {
    return octaryn_server_chunk_stream_write_decision{
        .use_previous_window = 0u,
        .should_write = 1u,
    };
  }

  return state->decide(
      metadata_only != 0u, submitted_block_commands != 0u,
      chunk_stream_window{.center_chunk_x = center_chunk_x,
                          .center_chunk_z = center_chunk_z,
                          .radius = radius},
      has_previous_window != 0u,
      chunk_stream_window{.center_chunk_x = previous_center_chunk_x,
                          .center_chunk_z = previous_center_chunk_z,
                          .radius = previous_radius});
}

void octaryn_server_chunk_stream_write_tracker_note_written(
    void *tracker, int32_t center_chunk_x, int32_t center_chunk_z,
    uint32_t radius) {
  auto *state = as_tracker(tracker);
  if (state == nullptr) {
    return;
  }

  state->note_written(chunk_stream_window{.center_chunk_x = center_chunk_x,
                                          .center_chunk_z = center_chunk_z,
                                          .radius = radius});
}

int32_t octaryn_server_chunk_stream_plan_process_write(
    void *tracker, int32_t intent_read_result, uint32_t allow_transient_invalid,
    const octaryn_server_chunk_view_intent *intent, uint32_t metadata_only,
    uint32_t submitted_block_commands,
    octaryn_server_chunk_stream_process_write_plan *plan) {
  if (plan == nullptr) {
    return -1;
  }

  *plan = {};
  if (!map_intent_read_result(intent_read_result, allow_transient_invalid != 0u,
                              *plan)) {
    return 0;
  }

  if (intent == nullptr) {
    *plan = stop_plan(process_write_plan_reason_intent_read_failed, -1);
    return 0;
  }

  const auto decision = octaryn_server_chunk_stream_write_tracker_decide(
      tracker, metadata_only, submitted_block_commands, intent->center_chunk_x,
      intent->center_chunk_z, intent->radius, intent->has_previous_window,
      intent->previous_center_chunk_x, intent->previous_center_chunk_z,
      intent->previous_radius);

  *plan = octaryn_server_chunk_stream_process_write_plan{
      .should_continue = 1u,
      .should_write = decision.should_write,
      .use_previous_window = decision.use_previous_window,
      .reason = decision.should_write == 0u
                    ? process_write_plan_reason_unchanged_window
                    : process_write_plan_reason_none,
      .handle_result = 0,
      .center_chunk_x = intent->center_chunk_x,
      .center_chunk_z = intent->center_chunk_z,
      .radius = intent->radius,
  };
  return 0;
}

int32_t octaryn_server_chunk_stream_read_process_intent(
    const char *intent_path, uint32_t allow_transient_invalid,
    octaryn_server_chunk_view_intent *intent,
    octaryn_server_chunk_stream_process_write_plan *plan) {
  if (intent == nullptr || plan == nullptr) {
    return -1;
  }

  *intent = {};
  *plan = {};
  const int32_t read_result =
      octaryn_server_chunk_stream_read_view_intent(intent_path, intent);
  if (!map_intent_read_result(read_result, allow_transient_invalid != 0u,
                              *plan)) {
    return 0;
  }

  *plan = octaryn_server_chunk_stream_process_write_plan{
      .should_continue = 1u,
      .should_write = 1u,
      .use_previous_window = 0u,
      .reason = process_write_plan_reason_none,
      .handle_result = 0,
      .center_chunk_x = intent->center_chunk_x,
      .center_chunk_z = intent->center_chunk_z,
      .radius = intent->radius,
  };
  return 0;
}

void octaryn_server_chunk_stream_process_write_plan_note_written(
    void *tracker, const octaryn_server_chunk_stream_process_write_plan *plan) {
  if (plan == nullptr || plan->should_write == 0u) {
    return;
  }

  octaryn_server_chunk_stream_write_tracker_note_written(
      tracker, plan->center_chunk_x, plan->center_chunk_z, plan->radius);
}

octaryn_server_chunk_stream_process_tick_decision
octaryn_server_chunk_stream_decide_process_tick(uint32_t has_player_input,
                                                uint32_t submitted_commands,
                                                uint32_t metadata_only) {
  const bool uses_player_input = has_player_input != 0u;
  const bool has_submitted_commands = submitted_commands != 0u;
  const bool should_tick = uses_player_input || has_submitted_commands;
  return octaryn_server_chunk_stream_process_tick_decision{
      .should_tick = should_tick ? 1u : 0u,
      .use_host_only_tick = metadata_only != 0u ? 1u : 0u,
      .use_default_frame = should_tick && !uses_player_input ? 1u : 0u,
  };
}

int32_t octaryn_server_chunk_stream_create_process_frame(
    octaryn_host_frame_snapshot *frame) {
  if (frame == nullptr) {
    return -1;
  }

  *frame = {};
  frame->version = 1u;
  frame->size = OCTARYN_HOST_FRAME_SNAPSHOT_SIZE;
  frame->input.version = 1u;
  frame->input.size = OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
  frame->timing.version = 1u;
  frame->timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
  frame->timing.frame_index = 1u;
  frame->timing.delta_seconds = 1.0 / 60.0;
  return 0;
}
}
