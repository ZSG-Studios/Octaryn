#include "BlockCommandQueue.h"

#include <cmath>
#include <limits>
#include <vector>

namespace octaryn::server::world::blocks {

namespace {

inline constexpr float ClientInteractionReach = 6.0f;
inline constexpr float ClientInteractionReachSquared =
    ClientInteractionReach * ClientInteractionReach;

bool is_finite_interaction(const octaryn_host_command &command) {
  return std::isfinite(command.x) && std::isfinite(command.y) &&
         std::isfinite(command.z) && std::isfinite(command.x2) &&
         std::isfinite(command.y2) && std::isfinite(command.z2);
}

int32_t rounded_block_coordinate(float value) {
  return static_cast<int32_t>(std::nearbyint(value));
}

int32_t manhattan_distance(const BlockPosition &left,
                           const BlockPosition &right) {
  return std::abs(left.x - right.x) + std::abs(left.y - right.y) +
         std::abs(left.z - right.z);
}

} // namespace

size_t ClientBlockCommandQueue::pending_count() const {
  return commands_.size();
}

bool ClientBlockCommandQueue::can_queue(
    const octaryn_host_command &command,
    const BlockCommandQueuePolicy &policy) const {
  if (commands_.size() >= MaxPendingClientBlockCommands ||
      !host_command_is_supported_set_block(command)) {
    return false;
  }

  const uint16_t block = host_command_block(command);
  if (block != AirBlock &&
      (!policy.is_client_placeable || !policy.is_client_placeable(block))) {
    return false;
  }

  return policy.can_apply && policy.can_apply(command);
}

int ClientBlockCommandQueue::submit(const octaryn_host_command *commands,
                                    size_t command_count,
                                    const BlockCommandQueuePolicy &policy,
                                    size_t &rejected_index) {
  rejected_index = 0u;
  if ((command_count > 0u && commands == nullptr) ||
      command_count > MaxPendingClientBlockCommands ||
      commands_.size() > MaxPendingClientBlockCommands - command_count) {
    return -1;
  }

  for (size_t index = 0u; index < command_count; ++index) {
    if (!can_queue(commands[index], policy)) {
      rejected_index = index;
      return -2;
    }
  }

  for (size_t index = 0u; index < command_count; ++index) {
    commands_.push(commands[index]);
  }
  return 0;
}

ClientBlockCommandSubmitReport
ClientBlockCommandQueue::submit_report(const octaryn_host_command *commands,
                                       size_t command_count,
                                       const BlockCommandQueuePolicy &policy) {
  ClientBlockCommandSubmitReport report{};
  report.requested_count = static_cast<uint32_t>(command_count);
  report.pending_before = pending_count();
  size_t rejected = 0u;
  report.result = submit(commands, command_count, policy, rejected);
  report.rejected_index = static_cast<uint32_t>(rejected);
  report.pending_after = pending_count();
  if (report.result == -1) {
    report.reason = ClientBlockCommandSubmitReason::capacity;
  } else if (report.result == -2) {
    report.reason = ClientBlockCommandSubmitReason::rejected_command;
  } else if (report.result != 0) {
    report.reason = ClientBlockCommandSubmitReason::invalid_queue;
  }
  return report;
}

int ClientBlockCommandQueue::drain(
    const std::function<bool(const octaryn_host_command &command)>
        &apply_command) {
  int applied = 0;
  while (!commands_.empty()) {
    const octaryn_host_command command = commands_.front();
    commands_.pop();
    if (apply_command && apply_command(command)) {
      ++applied;
    }
  }
  return applied;
}

int ClientBlockCommandQueue::drain_apply(
    BlockStore &store, const BlockEditPolicy &policy,
    const std::function<void(const octaryn_host_command &command,
                             const BlockEditApplyResult &result)> &on_result) {
  int applied = 0;
  while (!commands_.empty()) {
    const octaryn_host_command command = commands_.front();
    commands_.pop();
    const BlockEditApplyResult result =
        apply_block_command(store, command, policy);
    if (result.result.applied) {
      ++applied;
    }

    if (on_result) {
      on_result(command, result);
    }
  }
  return applied;
}

bool host_command_is_current(const octaryn_host_command &command) {
  return command.version == HostCommandVersion &&
         command.size == OCTARYN_HOST_COMMAND_SIZE;
}

bool host_command_is_client_interaction(const octaryn_host_command &command) {
  return (command.flags & OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG) != 0u;
}

bool host_command_is_supported_set_block(const octaryn_host_command &command) {
  return host_command_is_current(command) &&
         command.kind == HostCommandSetBlockKind && command.d >= 0 &&
         command.d <= std::numeric_limits<uint16_t>::max();
}

uint16_t host_command_block(const octaryn_host_command &command) {
  return static_cast<uint16_t>(command.d);
}

BlockPosition
host_command_interaction_hit_position(const octaryn_host_command &command) {
  return BlockPosition{
      .x = rounded_block_coordinate(command.x2),
      .y = rounded_block_coordinate(command.y2),
      .z = rounded_block_coordinate(command.z2),
  };
}

bool host_command_client_interaction_is_valid(
    const octaryn_host_command &command, uint16_t hit_block,
    uint16_t edit_position_block) {
  if (!host_command_is_client_interaction(command)) {
    return true;
  }

  if (!host_command_is_supported_set_block(command) ||
      !is_finite_interaction(command) || hit_block == AirBlock) {
    return false;
  }

  const BlockPosition edit_position{
      .x = command.a,
      .y = command.b,
      .z = command.c,
  };
  const BlockPosition hit_position =
      host_command_interaction_hit_position(command);
  const float hit_center_x = static_cast<float>(hit_position.x) + 0.5f;
  const float hit_center_y = static_cast<float>(hit_position.y) + 0.5f;
  const float hit_center_z = static_cast<float>(hit_position.z) + 0.5f;
  const float delta_x = command.x - hit_center_x;
  const float delta_y = command.y - hit_center_y;
  const float delta_z = command.z - hit_center_z;
  const float reach_squared =
      delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
  if (reach_squared > ClientInteractionReachSquared) {
    return false;
  }

  if (host_command_block(command) == AirBlock) {
    return edit_position == hit_position;
  }

  return manhattan_distance(edit_position, hit_position) == 1 &&
         edit_position_block == AirBlock;
}

} // namespace octaryn::server::world::blocks

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
      });
}

uint32_t octaryn_server_client_block_command_hit_position(
    const octaryn_host_command *command,
    octaryn_server_block_position *position) {
  if (command == nullptr || position == nullptr ||
      !octaryn::server::world::blocks::is_finite_interaction(*command)) {
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

uint32_t octaryn_server_host_command_is_current(
    const octaryn_host_command *command) {
  return command != nullptr &&
                 octaryn::server::world::blocks::host_command_is_current(
                     *command)
             ? 1u
             : 0u;
}
}
