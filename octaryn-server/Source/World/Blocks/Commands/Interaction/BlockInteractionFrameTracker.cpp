#include "ChunkColumnStream.h"

#include <cstdint>

namespace {

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

void octaryn_server_block_interaction_frame_tracker_note_submitted(
    void *tracker, uint64_t frame_index) {
  auto *state = as_tracker(tracker);
  if (state == nullptr) {
    return;
  }

  state->note_submitted(frame_index);
}

}
