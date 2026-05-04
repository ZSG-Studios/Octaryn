#pragma once

#include "HostExports.h"

#include <cstdint>

namespace octaryn_client_app {

constexpr uint32_t kHostCommandCriticalFlag = 1u;
constexpr uint32_t kHostCommandClientInteractionFlag = 1u << 1u;

struct client_command_frame_counts {
  uint32_t enqueued = 0u;
  uint32_t set_block = 0u;
  uint32_t place_block = 0u;
  uint32_t break_block = 0u;
};

void reset_command_frame_counts();
const client_command_frame_counts &command_frame_counts();
int OCTARYN_ABI_CALL enqueue_command(octaryn_host_command *command);

} // namespace octaryn_client_app
