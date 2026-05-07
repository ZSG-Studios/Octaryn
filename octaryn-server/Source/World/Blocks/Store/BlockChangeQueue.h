#pragma once

#include "BlockStore.h"
#include "octaryn_shared_abi_types.h"

#include <cstddef>
#include <cstdint>
#include <queue>
#include <vector>

namespace octaryn::server::world::blocks {

inline constexpr uint32_t ReplicationChangeVersion = 1u;
inline constexpr uint32_t ReplicationChangeSize = 40u;
inline constexpr uint32_t BlockEditChangeKind = 1u;

struct ReplicationChange {
  uint32_t version;
  uint32_t size;
  uint32_t change_kind;
  uint32_t flags;
  uint64_t replication_id;
  uint64_t payload0;
  uint64_t payload1;
};

class BlockChangeQueue {
public:
  [[nodiscard]] size_t pending_count() const;

  void enqueue(const BlockEdit &edit);
  void enqueue_all(const std::vector<BlockEdit> &edits);
  int drain(ReplicationChange *changes, uint32_t capacity, uint64_t tick_id,
            uint32_t &written);

private:
  std::queue<BlockEdit> changes_;
};

[[nodiscard]] ReplicationChange to_replication_change(const BlockEdit &edit,
                                                      uint64_t tick_id);

} // namespace octaryn::server::world::blocks

extern "C" {

OCTARYN_SERVER_BLOCK_STORE_API void *
octaryn_server_block_change_queue_create();

OCTARYN_SERVER_BLOCK_STORE_API void
octaryn_server_block_change_queue_destroy(void *queue);

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_block_change_queue_pending_count(void *queue);

OCTARYN_SERVER_BLOCK_STORE_API void
octaryn_server_block_change_queue_enqueue(
    void *queue, const octaryn_server_block_edit *edit);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_block_change_queue_drain(
    void *queue, octaryn::server::world::blocks::ReplicationChange *changes,
    uint32_t capacity, uint64_t tick_id, uint32_t *written);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_block_change_queue_drain_snapshot(
    void *queue, octaryn_server_snapshot_header *snapshot_header,
    uint64_t tick_id, uint64_t *pending_before, uint32_t *written);

}
