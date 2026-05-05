#include "BlockCommandQueue.h"

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

uint64_t octaryn_server_client_block_command_queue_pending_count(void *queue) {
  const auto *commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  return commands == nullptr ? 0u : commands->pending_count();
}

int32_t octaryn_server_client_block_command_queue_submit(
    void *queue, const octaryn_host_command *commands, uint32_t command_count,
    octaryn_server_block_placeable_fn is_client_placeable,
    octaryn_server_block_command_fn can_apply, void *context,
    uint32_t *rejected_index) {
  auto *queue_commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  if (queue_commands == nullptr ||
      (command_count > 0u && commands == nullptr)) {
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
  size_t rejected = 0u;
  const int result =
      queue_commands->submit(commands, command_count, policy, rejected);
  if (rejected_index != nullptr) {
    *rejected_index = static_cast<uint32_t>(rejected);
  }
  return result;
}

int32_t octaryn_server_client_block_command_queue_drain(
    void *queue, octaryn_server_block_command_fn apply_command, void *context) {
  auto *commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  if (commands == nullptr) {
    return -1;
  }

  return commands->drain(
      [apply_command, context](const octaryn_host_command &value) {
        return apply_command != nullptr && apply_command(context, &value) != 0u;
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
}
