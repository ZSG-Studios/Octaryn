#include "BlockCommandQueue.h"

#include "BlockStore.h"

#include <limits>

namespace octaryn::server::world::blocks {

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

  return host_command_is_client_interaction(command) ||
         (policy.can_apply && policy.can_apply(command));
}

bool ClientBlockCommandQueue::enqueue(const octaryn_host_command &command,
                                      const BlockCommandQueuePolicy &policy) {
  if (!can_queue(command, policy)) {
    return false;
  }

  commands_.push(command);
  return true;
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

uint32_t octaryn_server_client_block_command_queue_can_queue(
    void *queue, const octaryn_host_command *command,
    octaryn_server_block_placeable_fn is_client_placeable,
    octaryn_server_block_command_fn can_apply, void *context) {
  const auto *commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  if (commands == nullptr || command == nullptr) {
    return 0u;
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
  return commands->can_queue(*command, policy) ? 1u : 0u;
}

uint32_t octaryn_server_client_block_command_queue_enqueue(
    void *queue, const octaryn_host_command *command,
    octaryn_server_block_placeable_fn is_client_placeable,
    octaryn_server_block_command_fn can_apply, void *context) {
  auto *commands =
      static_cast<octaryn::server::world::blocks::ClientBlockCommandQueue *>(
          queue);
  if (commands == nullptr || command == nullptr) {
    return 0u;
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
  return commands->enqueue(*command, policy) ? 1u : 0u;
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

}
