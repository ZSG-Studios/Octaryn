#pragma once

#include "BlockStore.h"
#include "octaryn_shared_abi_types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>

#if !defined(OCTARYN_SERVER_BLOCK_STORE_API)
#if defined(_WIN32)
#define OCTARYN_SERVER_BLOCK_STORE_API __declspec(dllexport)
#else
#define OCTARYN_SERVER_BLOCK_STORE_API __attribute__((visibility("default")))
#endif
#endif

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
[[nodiscard]] bool
host_command_client_interaction_is_valid(const octaryn_host_command &command,
                                         uint16_t hit_block,
                                         uint16_t edit_position_block);

} // namespace octaryn::server::world::blocks

extern "C" {

using octaryn_server_block_placeable_fn = uint32_t (*)(void *context,
                                                       uint16_t block);
using octaryn_server_block_command_fn =
    uint32_t (*)(void *context, const octaryn_host_command *command);

OCTARYN_SERVER_BLOCK_STORE_API void *
octaryn_server_client_block_command_queue_create();

OCTARYN_SERVER_BLOCK_STORE_API void
octaryn_server_client_block_command_queue_destroy(void *queue);

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_client_block_command_queue_pending_count(void *queue);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_client_block_command_queue_can_queue(
    void *queue, const octaryn_host_command *command,
    octaryn_server_block_placeable_fn is_client_placeable,
    octaryn_server_block_command_fn can_apply, void *context);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_client_block_command_queue_enqueue(
    void *queue, const octaryn_host_command *command,
    octaryn_server_block_placeable_fn is_client_placeable,
    octaryn_server_block_command_fn can_apply, void *context);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_client_block_command_queue_drain(
    void *queue, octaryn_server_block_command_fn apply_command, void *context);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_client_block_command_hit_position(
    const octaryn_host_command *command,
    octaryn_server_block_position *position);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_client_block_command_is_valid_interaction(
    const octaryn_host_command *command, uint16_t hit_block,
    uint16_t edit_position_block);
}
