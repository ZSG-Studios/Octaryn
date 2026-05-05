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
  [[nodiscard]] octaryn_server_chunk_stream_write_decision decide(
      bool metadata_only, bool submitted_block_commands,
      const chunk_stream_window &requested_window, bool has_previous_window,
      const chunk_stream_window &previous_window) const {
    const bool has_trusted_previous_window =
        written_windows_.contains(requested_window) ||
        (has_previous_window && written_windows_.contains(previous_window));
    const bool use_previous_window =
        metadata_only ? has_previous_window && has_trusted_previous_window
                      : has_previous_window;
    const bool should_skip =
        metadata_only && !submitted_block_commands && use_previous_window &&
        has_last_written_ && last_written_ == requested_window &&
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

}
