#include "HostCommands.h"

#include "Log.h"

#include <cinttypes>
#include <cstdio>

namespace octaryn_client_app {

namespace {

client_command_frame_counts g_command_frame_counts;

const char *command_edit_label(const octaryn_host_command &command) {
  if (command.kind != 1u) {
    return "none";
  }

  return command.d == 0 ? "break" : "place";
}

void count_enqueued_command(const octaryn_host_command &command) {
  ++g_command_frame_counts.enqueued;
  if (command.kind != 1u) {
    return;
  }

  ++g_command_frame_counts.set_block;
  if (command.d == 0) {
    ++g_command_frame_counts.break_block;
  } else {
    ++g_command_frame_counts.place_block;
  }
}

void log_client_command_enqueue(const octaryn_host_command &command) {
  if (g_log == nullptr) {
    return;
  }

  std::fprintf(g_log,
               "live_client_command_enqueue kind=%" PRIu32 " request=%" PRIu64
               " target=%" PRIu64 " edit=%s block=(%" PRId32 ",%" PRId32
               ",%" PRId32 ",%" PRId32 ") flags=%" PRIu32 "\n",
               command.kind, command.request_id, command.target_id,
               command_edit_label(command), command.a, command.b, command.c,
               command.d, command.flags);
  std::fflush(g_log);
}

} // namespace

void reset_command_frame_counts() { g_command_frame_counts = {}; }

const client_command_frame_counts &command_frame_counts() {
  return g_command_frame_counts;
}

int OCTARYN_ABI_CALL enqueue_command(octaryn_host_command *command) {
  if (command != nullptr) {
    count_enqueued_command(*command);
    log_client_command_enqueue(*command);
  }

  return 1;
}

} // namespace octaryn_client_app
