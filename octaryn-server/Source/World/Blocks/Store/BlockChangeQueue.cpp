#include "BlockChangeQueue.h"

namespace octaryn::server::world::blocks {
namespace {

uint64_t pack_signed_pair(int32_t a, int32_t b) {
  return static_cast<uint32_t>(a) |
         (static_cast<uint64_t>(static_cast<uint32_t>(b)) << 32u);
}

uint64_t pack_block(int32_t z, uint16_t block) {
  return static_cast<uint32_t>(z) | (static_cast<uint64_t>(block) << 32u);
}

} // namespace

size_t BlockChangeQueue::pending_count() const { return changes_.size(); }

void BlockChangeQueue::enqueue(const BlockEdit &edit) { changes_.push(edit); }

int BlockChangeQueue::drain(ReplicationChange *changes, uint32_t capacity,
                            uint64_t tick_id, uint32_t &written) {
  written = 0;
  if (changes_.empty()) {
    return 0;
  }

  if (changes == nullptr || capacity < changes_.size()) {
    return -1;
  }

  while (!changes_.empty()) {
    changes[written++] = to_replication_change(changes_.front(), tick_id);
    changes_.pop();
  }
  return 0;
}

ReplicationChange to_replication_change(const BlockEdit &edit,
                                        uint64_t tick_id) {
  return ReplicationChange{
      .version = ReplicationChangeVersion,
      .size = ReplicationChangeSize,
      .change_kind = BlockEditChangeKind,
      .flags = 0,
      .replication_id = tick_id,
      .payload0 = pack_signed_pair(edit.position.x, edit.position.y),
      .payload1 = pack_block(edit.position.z, edit.block),
  };
}

} // namespace octaryn::server::world::blocks
