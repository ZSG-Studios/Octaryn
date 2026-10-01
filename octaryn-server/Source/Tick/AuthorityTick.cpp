#include "AuthorityTick.h"

#include "octaryn_native_schedule_policy.h"

#include <array>
#include <iterator>

namespace {

constexpr const char *AuthorityTickJobId = "server.authority.tick";
constexpr std::array<octaryn_native_schedule_resource_access, 3> AuthorityAccesses = {{
    {"server.client_commands.queue", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE},
    {"server.player.state", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE},
    {"server.world_time.clock", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}}};

int execute_authority(void *context) {
  const auto &callbacks = *static_cast<const octaryn_server_authority_tick_callbacks *>(context);
  int result = callbacks.command_drain(callbacks.command_drain_context);
  if (result == 0) result = callbacks.player_tick(callbacks.player_context);
  if (result == 0) result = callbacks.world_time_tick(callbacks.world_time_context);
  return result;
}

bool valid_callbacks(const octaryn_server_authority_tick_callbacks *callbacks) {
  return callbacks != nullptr && callbacks->command_drain != nullptr &&
         callbacks->player_tick != nullptr &&
         callbacks->world_time_tick != nullptr;
}

} // namespace

int octaryn_server_authority_tick_execute(
    void *schedule_runtime,
    const octaryn_server_authority_tick_callbacks *callbacks,
    octaryn_native_schedule_runtime_report *report) {
  if (schedule_runtime == nullptr || !valid_callbacks(callbacks)) {
    return -1;
  }

  // These phases are strictly serial and consumed immediately by this owner.
  // Worker dispatch adds a scheduling round trip without independent work.
  const octaryn_native_schedule_runtime_job job = {
      AuthorityTickJobId, AuthorityAccesses.data(), AuthorityAccesses.size(),
      nullptr, 0, OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_MAIN_THREAD, execute_authority,
      const_cast<octaryn_server_authority_tick_callbacks *>(callbacks)};
  return octaryn_native_schedule_runtime_execute(schedule_runtime, &job, 1, report);
}

int octaryn_server_authority_tick_validate_report(
    const octaryn_native_schedule_runtime_report *report) {
  if (report == nullptr) {
    return -1;
  }

  return report->submitted_jobs == 1 &&
                 report->completed_jobs == 1 &&
                 report->worker_jobs == 0 &&
                 report->main_thread_jobs == 1 && report->execution_waves == 1 &&
                 report->failed_job_index == -1
             ? 0
             : -2;
}
