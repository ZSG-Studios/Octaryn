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

extern "C" {

void *octaryn_server_block_change_queue_create() {
  return new octaryn::server::world::blocks::BlockChangeQueue();
}

void octaryn_server_block_change_queue_destroy(void *queue) {
  delete static_cast<octaryn::server::world::blocks::BlockChangeQueue *>(queue);
}

uint64_t octaryn_server_block_change_queue_pending_count(void *queue) {
  const auto *changes =
      static_cast<octaryn::server::world::blocks::BlockChangeQueue *>(queue);
  return changes == nullptr ? 0u : changes->pending_count();
}

void octaryn_server_block_change_queue_enqueue(
    void *queue, const octaryn_server_block_edit *edit) {
  auto *changes =
      static_cast<octaryn::server::world::blocks::BlockChangeQueue *>(queue);
  if (changes == nullptr || edit == nullptr) {
    return;
  }

  changes->enqueue(octaryn::server::world::blocks::BlockEdit{
      .position =
          octaryn::server::world::blocks::BlockPosition{
              .x = edit->position.x,
              .y = edit->position.y,
              .z = edit->position.z,
          },
      .block = edit->block,
  });
}

int32_t octaryn_server_block_change_queue_drain(
    void *queue, octaryn::server::world::blocks::ReplicationChange *changes,
    uint32_t capacity, uint64_t tick_id, uint32_t *written) {
  auto *change_queue =
      static_cast<octaryn::server::world::blocks::BlockChangeQueue *>(queue);
  if (written == nullptr) {
    return -1;
  }

  if (change_queue == nullptr) {
    *written = 0u;
    return -1;
  }

  return change_queue->drain(changes, capacity, tick_id, *written);
}

}
