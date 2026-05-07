#include "BlockCommandQueue.h"
#include "BlockChangeQueue.h"

#include <cmath>
#include <limits>

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
  return drain_apply_and_enqueue(store, nullptr, policy, on_result);
}

int ClientBlockCommandQueue::drain_apply_and_enqueue(
    BlockStore &store, BlockChangeQueue *change_queue,
    const BlockEditPolicy &policy,
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
    if (change_queue != nullptr) {
      change_queue->enqueue_all(result.changes);
    }

    if (on_result) {
      on_result(command, result);
    }
  }
  return applied;
}

const char *
client_block_command_submit_reason_name(ClientBlockCommandSubmitReason reason) {
  switch (reason) {
  case ClientBlockCommandSubmitReason::accepted:
    return "accepted";
  case ClientBlockCommandSubmitReason::capacity:
    return "capacity";
  case ClientBlockCommandSubmitReason::rejected_command:
    return "rejected_command";
  case ClientBlockCommandSubmitReason::invalid_queue:
    return "native_submit";
  }
  return "native_submit";
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
