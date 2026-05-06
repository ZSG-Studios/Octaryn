#pragma once

#include "BlockEditService.h"
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

enum class ClientBlockCommandSubmitReason : uint32_t {
  accepted = 0u,
  capacity = 1u,
  rejected_command = 2u,
  invalid_queue = 3u,
};

struct ClientBlockCommandSubmitReport {
  int32_t result = 0;
  uint32_t rejected_index = 0u;
  ClientBlockCommandSubmitReason reason =
      ClientBlockCommandSubmitReason::accepted;
  uint32_t requested_count = 0u;
  uint64_t pending_before = 0u;
  uint64_t pending_after = 0u;
};

class ClientBlockCommandQueue {
public:
  [[nodiscard]] size_t pending_count() const;

  int submit(const octaryn_host_command *commands, size_t command_count,
             const BlockCommandQueuePolicy &policy, size_t &rejected_index);
  ClientBlockCommandSubmitReport
  submit_report(const octaryn_host_command *commands, size_t command_count,
                const BlockCommandQueuePolicy &policy);
  int drain(const std::function<bool(const octaryn_host_command &command)>
                &apply_command);
  int drain_apply(
      BlockStore &store, const BlockEditPolicy &policy,
      const std::function<void(const octaryn_host_command &command,
                               const BlockEditApplyResult &result)> &on_result);

private:
  [[nodiscard]] bool can_queue(const octaryn_host_command &command,
                               const BlockCommandQueuePolicy &policy) const;

  std::queue<octaryn_host_command> commands_;
};

[[nodiscard]] bool host_command_is_current(const octaryn_host_command &command);
[[nodiscard]] bool
host_command_is_client_interaction(const octaryn_host_command &command);
[[nodiscard]] bool
host_command_is_supported_set_block(const octaryn_host_command &command);
[[nodiscard]] uint16_t host_command_block(const octaryn_host_command &command);
[[nodiscard]] BlockPosition
host_command_interaction_hit_position(const octaryn_host_command &command);
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
using octaryn_server_block_command_result_fn = uint32_t (*)(
    void *context, const octaryn_host_command *command,
    const octaryn_server_block_edit_result *result,
    const octaryn_server_block_edit *changes, uint32_t change_count);

struct octaryn_server_client_block_command_submit_report {
  int32_t result;
  uint32_t rejected_index;
  uint32_t reason;
  uint32_t requested_count;
  uint64_t pending_before;
  uint64_t pending_after;
};

OCTARYN_SERVER_BLOCK_STORE_API void *
octaryn_server_client_block_command_queue_create();

OCTARYN_SERVER_BLOCK_STORE_API void
octaryn_server_client_block_command_queue_destroy(void *queue);

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_client_block_command_queue_max_pending();

OCTARYN_SERVER_BLOCK_STORE_API uint64_t
octaryn_server_client_block_command_queue_pending_count(void *queue);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_client_block_command_queue_submit(
    void *queue, const octaryn_host_command *commands, uint32_t command_count,
    octaryn_server_block_placeable_fn is_client_placeable,
    octaryn_server_block_command_fn can_apply, void *context,
    uint32_t *rejected_index);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_client_block_command_queue_submit_report(
    void *queue, const octaryn_host_command *commands, uint32_t command_count,
    octaryn_server_block_placeable_fn is_client_placeable,
    octaryn_server_block_command_fn can_apply, void *context,
    octaryn_server_client_block_command_submit_report *report);

OCTARYN_SERVER_BLOCK_STORE_API int32_t
octaryn_server_client_block_command_queue_drain_apply(
    void *queue, void *store, octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *policy_context, octaryn_server_block_command_result_fn on_result,
    void *result_context);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_client_block_command_hit_position(
    const octaryn_host_command *command,
    octaryn_server_block_position *position);

OCTARYN_SERVER_BLOCK_STORE_API uint32_t
octaryn_server_client_block_command_is_valid_interaction(
    const octaryn_host_command *command, uint16_t hit_block,
    uint16_t edit_position_block);
}
