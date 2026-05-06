#include "ChunkColumnStream.h"

#include <cstdint>

namespace {

enum block_interaction_process_plan_reason : uint32_t {
  block_interaction_process_plan_reason_none = 0u,
  block_interaction_process_plan_reason_missing_intent = 1u,
  block_interaction_process_plan_reason_intent_read_retry = 2u,
  block_interaction_process_plan_reason_partial_intent = 3u,
  block_interaction_process_plan_reason_unsupported_intent = 4u,
  block_interaction_process_plan_reason_intent_read_failed = 5u,
  block_interaction_process_plan_reason_duplicate_frame = 6u,
};

class block_interaction_frame_tracker {
public:
  [[nodiscard]] octaryn_server_block_interaction_frame_decision
  decide(uint64_t frame_index) const {
    if (frame_index == 0u) {
      return octaryn_server_block_interaction_frame_decision{
          .should_submit = 0u,
          .duplicate_frame = 0u,
      };
    }

    const bool duplicate = frame_index <= last_submitted_frame_;
    return octaryn_server_block_interaction_frame_decision{
        .should_submit = duplicate ? 0u : 1u,
        .duplicate_frame = duplicate ? 1u : 0u,
    };
  }

  void note_submitted(uint64_t frame_index) {
    if (frame_index > last_submitted_frame_) {
      last_submitted_frame_ = frame_index;
    }
  }

private:
  uint64_t last_submitted_frame_ = 0u;
};

block_interaction_frame_tracker *as_tracker(void *tracker) {
  return static_cast<block_interaction_frame_tracker *>(tracker);
}

octaryn_server_block_interaction_process_plan stop_plan(uint32_t reason,
                                                        int32_t handle_result) {
  return octaryn_server_block_interaction_process_plan{
      .should_continue = handle_result == 0 ? 1u : 0u,
      .should_submit = 0u,
      .reason = reason,
      .handle_result = handle_result,
      .frame_index = 0u,
      .command_count = 0u,
      .break_command_count = 0u,
      .place_command_count = 0u,
  };
}

} // namespace

extern "C" {

void *octaryn_server_block_interaction_frame_tracker_create() {
  return new block_interaction_frame_tracker();
}

void octaryn_server_block_interaction_frame_tracker_destroy(void *tracker) {
  delete as_tracker(tracker);
}

octaryn_server_block_interaction_frame_decision
octaryn_server_block_interaction_frame_tracker_decide(void *tracker,
                                                      uint64_t frame_index) {
  const auto *state = as_tracker(tracker);
  if (state == nullptr) {
    return octaryn_server_block_interaction_frame_decision{
        .should_submit = 0u,
        .duplicate_frame = 0u,
    };
  }

  return state->decide(frame_index);
}

int32_t octaryn_server_block_interaction_plan_process_intent(
    void *tracker, int32_t intent_read_result,
    uint32_t allow_transient_invalid,
    const octaryn_server_block_interaction_intent_result *intent,
    octaryn_server_block_interaction_process_plan *plan) {
  if (plan == nullptr) {
    return -1;
  }

  const bool allow_transient = allow_transient_invalid != 0u;
  switch (intent_read_result) {
  case 0:
    break;
  case 1:
    *plan =
        stop_plan(block_interaction_process_plan_reason_missing_intent, 0);
    return 0;
  case -2:
    *plan = stop_plan(block_interaction_process_plan_reason_intent_read_retry,
                      allow_transient ? 0 : -1);
    return 0;
  case -3:
    *plan = stop_plan(block_interaction_process_plan_reason_partial_intent,
                      allow_transient ? 0 : -1);
    return 0;
  case -4:
    *plan =
        stop_plan(block_interaction_process_plan_reason_unsupported_intent, -1);
    return 0;
  default:
    *plan =
        stop_plan(block_interaction_process_plan_reason_intent_read_failed, -1);
    return 0;
  }

  if (intent == nullptr) {
    *plan =
        stop_plan(block_interaction_process_plan_reason_intent_read_failed, -1);
    return 0;
  }

  const auto decision =
      octaryn_server_block_interaction_frame_tracker_decide(
          tracker, intent->frame_index);
  if (decision.should_submit == 0u) {
    *plan =
        stop_plan(block_interaction_process_plan_reason_duplicate_frame, 0);
    plan->frame_index = intent->frame_index;
    return 0;
  }

  *plan = octaryn_server_block_interaction_process_plan{
      .should_continue = 1u,
      .should_submit = 1u,
      .reason = block_interaction_process_plan_reason_none,
      .handle_result = 0,
      .frame_index = intent->frame_index,
      .command_count = intent->command_count,
      .break_command_count = intent->break_command_count,
      .place_command_count = intent->place_command_count,
  };
  return 0;
}

void octaryn_server_block_interaction_frame_tracker_note_submitted(
    void *tracker, uint64_t frame_index) {
  auto *state = as_tracker(tracker);
  if (state == nullptr) {
    return;
  }

  state->note_submitted(frame_index);
}

}
