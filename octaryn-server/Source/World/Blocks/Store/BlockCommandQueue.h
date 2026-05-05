#pragma once

#include "octaryn_shared_abi_types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>

namespace octaryn::server::world::blocks {

inline constexpr size_t MaxPendingClientBlockCommands = 4096u;
inline constexpr uint32_t HostCommandVersion = 1u;
inline constexpr uint32_t HostCommandSetBlockKind = 1u;

struct BlockCommandQueuePolicy {
  std::function<bool(uint16_t block)> is_client_placeable;
  std::function<bool(const octaryn_host_command &command)> can_apply;
};

class ClientBlockCommandQueue {
public:
  [[nodiscard]] size_t pending_count() const;
  [[nodiscard]] bool can_queue(const octaryn_host_command &command,
                               const BlockCommandQueuePolicy &policy) const;

  bool enqueue(const octaryn_host_command &command,
               const BlockCommandQueuePolicy &policy);
  int drain(const std::function<bool(const octaryn_host_command &command)>
                &apply_command);

private:
  std::queue<octaryn_host_command> commands_;
};

[[nodiscard]] bool host_command_is_current(const octaryn_host_command &command);
[[nodiscard]] bool
host_command_is_client_interaction(const octaryn_host_command &command);
[[nodiscard]] bool
host_command_is_supported_set_block(const octaryn_host_command &command);
[[nodiscard]] uint16_t host_command_block(const octaryn_host_command &command);

} // namespace octaryn::server::world::blocks
