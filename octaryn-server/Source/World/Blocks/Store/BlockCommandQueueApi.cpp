#include "BlockCommandQueue.h"
#include "BlockChangeQueue.h"

#include <cmath>
#include <vector>

namespace {

bool is_finite_interaction(const octaryn_host_command &command) {
  return std::isfinite(command.x) && std::isfinite(command.y) &&
         std::isfinite(command.z) && std::isfinite(command.x2) &&
         std::isfinite(command.y2) && std::isfinite(command.z2);
}

void write_command_result(
    octaryn_server_block_command_result_fn on_result, void *result_context,
    const octaryn_host_command &command,
    const octaryn::server::world::blocks::BlockEditApplyResult &result) {
  if (on_result == nullptr) {
    return;
  }

  const auto native_result =
      octaryn::server::world::blocks::to_abi_result(result.result);
  std::vector<octaryn_server_block_edit> native_changes;
  native_changes.reserve(result.changes.size());
  for (const auto &change : result.changes) {
    native_changes.push_back(
        octaryn::server::world::blocks::to_abi_block_edit(change));
  }

  on_result(result_context, &command, &native_result,
            native_changes.empty() ? nullptr : native_changes.data(),
            static_cast<uint32_t>(native_changes.size()));
}

} // namespace

extern "C" {

void *octaryn_server_client_block_command_queue_create() {
  return new octaryn::server::world::blocks::ClientBlockCommandQueue();
}

void octaryn_server_client_block_command_queue_destroy(void *queue) {
  delete static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
      queue);
}

uint64_t octaryn_server_client_block_command_queue_max_pending() {
  return octaryn::server::world::blocks::MaxPendingClientBlockCommands;
}

uint64_t octaryn_server_client_block_command_queue_pending_count(void *queue) {
  const auto *commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  return commands == nullptr ? 0u : commands->pending_count();
}

int32_t octaryn_server_client_block_command_queue_submit_report(
    void *queue, const octaryn_host_command *commands, uint32_t command_count,
    octaryn_server_block_placeable_fn is_client_placeable,
    octaryn_server_block_command_fn can_apply, void *context,
    octaryn_server_client_block_command_submit_report *report) {
  auto *queue_commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  if (queue_commands == nullptr) {
    if (report != nullptr) {
      *report = octaryn_server_client_block_command_submit_report{
          .result = -1,
          .rejected_index = 0u,
          .reason = static_cast<uint32_t>(
              octaryn::server::world::blocks::ClientBlockCommandSubmitReason::
                  invalid_queue),
          .requested_count = command_count,
          .pending_before = 0u,
          .pending_after = 0u,
      };
    }
    return -1;
  }

  if (command_count > 0u && commands == nullptr) {
    const auto pending = queue_commands->pending_count();
    if (report != nullptr) {
      *report = octaryn_server_client_block_command_submit_report{
          .result = -1,
          .rejected_index = 0u,
          .reason = static_cast<uint32_t>(
              octaryn::server::world::blocks::ClientBlockCommandSubmitReason::
                  capacity),
          .requested_count = command_count,
          .pending_before = pending,
          .pending_after = pending,
      };
    }
    return -1;
  }

  const octaryn::server::world::blocks::BlockCommandQueuePolicy policy{
      .is_client_placeable =
          [is_client_placeable, context](uint16_t block) {
            return is_client_placeable != nullptr &&
                   is_client_placeable(context, block) != 0u;
          },
      .can_apply =
          [can_apply, context](const octaryn_host_command &value) {
            return can_apply != nullptr && can_apply(context, &value) != 0u;
          },
  };
  const auto native_report =
      queue_commands->submit_report(commands, command_count, policy);
  if (report != nullptr) {
    *report = octaryn_server_client_block_command_submit_report{
        .result = native_report.result,
        .rejected_index = native_report.rejected_index,
        .reason = static_cast<uint32_t>(native_report.reason),
        .requested_count = native_report.requested_count,
        .pending_before = native_report.pending_before,
        .pending_after = native_report.pending_after,
    };
  }
  return native_report.result;
}

int32_t octaryn_server_client_block_command_queue_drain_apply(
    void *queue, void *store, octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *policy_context, octaryn_server_block_command_result_fn on_result,
    void *result_context) {
  auto *commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  auto *block_store =
      static_cast<octaryn::server::world::blocks::BlockStore *>(store);
  if (commands == nullptr || block_store == nullptr) {
    return -1;
  }

  const auto policy = octaryn::server::world::blocks::policy_from_abi(
      generated_block, is_known_block, can_apply_edit, can_stay_supported,
      policy_context);
  return commands->drain_apply(
      *block_store, policy,
      [on_result, result_context](
          const octaryn_host_command &command,
          const octaryn::server::world::blocks::BlockEditApplyResult &result) {
        write_command_result(on_result, result_context, command, result);
      });
}

int32_t octaryn_server_client_block_command_queue_drain_apply_and_enqueue(
    void *queue, void *store, void *change_queue,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *policy_context, octaryn_server_block_command_result_fn on_result,
    void *result_context) {
  auto *commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  auto *block_store =
      static_cast<octaryn::server::world::blocks::BlockStore *>(store);
  auto *block_changes =
      static_cast<octaryn::server::world::blocks::BlockChangeQueue *>(
          change_queue);
  if (commands == nullptr || block_store == nullptr) {
    return -1;
  }

  const auto policy = octaryn::server::world::blocks::policy_from_abi(
      generated_block, is_known_block, can_apply_edit, can_stay_supported,
      policy_context);
  return commands->drain_apply_and_enqueue(
      *block_store, block_changes, policy,
      [on_result, result_context](
          const octaryn_host_command &command,
          const octaryn::server::world::blocks::BlockEditApplyResult &result) {
        write_command_result(on_result, result_context, command, result);
      });
}

int32_t
octaryn_server_client_block_command_queue_drain_apply_and_enqueue_report(
    void *queue, void *store, void *change_queue,
    octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *policy_context, octaryn_server_block_command_result_fn on_result,
    void *result_context,
    octaryn_server_client_block_command_drain_report *report) {
  if (report == nullptr) {
    return -1;
  }

  const int32_t applied =
      octaryn_server_client_block_command_queue_drain_apply_and_enqueue(
          queue, store, change_queue, generated_block, is_known_block,
          can_apply_edit, can_stay_supported, policy_context, on_result,
          result_context);
  const auto *commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  *report = octaryn_server_client_block_command_drain_report{
      .applied = applied,
      .pending_after = commands == nullptr ? 0u : commands->pending_count(),
  };
  return applied;
}

uint32_t octaryn_server_client_block_command_hit_position(
    const octaryn_host_command *command,
    octaryn_server_block_position *position) {
  if (command == nullptr || position == nullptr ||
      !is_finite_interaction(*command)) {
    return 0u;
  }

  const auto hit_position =
      octaryn::server::world::blocks::host_command_interaction_hit_position(
          *command);
  *position = octaryn_server_block_position{
      .x = hit_position.x,
      .y = hit_position.y,
      .z = hit_position.z,
  };
  return 1u;
}

uint32_t octaryn_server_client_block_command_is_valid_interaction(
    const octaryn_host_command *command, uint16_t hit_block,
    uint16_t edit_position_block) {
  return command != nullptr && octaryn::server::world::blocks::
                                   host_command_client_interaction_is_valid(
                                       *command, hit_block, edit_position_block)
             ? 1u
             : 0u;
}

const char *octaryn_server_client_block_command_edit_label(
    const octaryn_host_command *command) {
  if (command == nullptr ||
      command->kind !=
          octaryn::server::world::blocks::HostCommandSetBlockKind) {
    return "none";
  }

  return command->d == octaryn::server::world::blocks::AirBlock ? "break"
                                                                 : "place";
}

const char *octaryn_server_client_block_command_submit_reason_name(
    uint32_t reason) {
  return octaryn::server::world::blocks::
      client_block_command_submit_reason_name(
          static_cast<octaryn::server::world::blocks::
                          ClientBlockCommandSubmitReason>(reason));
}

uint32_t octaryn_server_host_command_is_current(
    const octaryn_host_command *command) {
  return command != nullptr &&
                 octaryn::server::world::blocks::host_command_is_current(
                     *command)
             ? 1u
             : 0u;
}
}
