#pragma once

#include "BlockStore.h"

#include <cstddef>
#include <cstdint>
#include <queue>

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
  int drain(ReplicationChange *changes, uint32_t capacity, uint64_t tick_id,
            uint32_t &written);

private:
  std::queue<BlockEdit> changes_;
};

[[nodiscard]] ReplicationChange to_replication_change(const BlockEdit &edit,
                                                      uint64_t tick_id);

} // namespace octaryn::server::world::blocks
