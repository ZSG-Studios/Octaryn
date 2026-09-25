#include "AuthorityTick.h"

#include "octaryn_native_schedule_policy.h"

#include <array>
#include <iterator>

namespace {

constexpr const char *PlayerTickJobId = "server.player.tick";
constexpr const char *CommandDrainJobId = "server.client_commands.drain";
constexpr const char *WorldTimeTickJobId = "server.world_time.advance";
constexpr size_t AuthorityTickJobCount = 3;
constexpr const char *PlayerTickDependencies[] = {CommandDrainJobId};
constexpr const char *WorldTimeDependencies[] = {PlayerTickJobId};

constexpr std::array<octaryn_native_schedule_resource_access, 2>
    CommandDrainAccesses = {
        {{"server.client_commands.queue", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE},
         {"server.block.store", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}}};
constexpr std::array<octaryn_native_schedule_resource_access, 1>
    PlayerTickAccesses = {
        {{"server.player.state", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}}};
constexpr std::array<octaryn_native_schedule_resource_access, 1>
    WorldTimeAccesses = {
        {{"server.world_time.clock", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}}};

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

  const octaryn_native_schedule_runtime_job jobs[] = {
      {CommandDrainJobId, CommandDrainAccesses.data(),
       CommandDrainAccesses.size(), nullptr, 0,
       OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE, callbacks->command_drain,
       callbacks->command_drain_context},
      {PlayerTickJobId, PlayerTickAccesses.data(), PlayerTickAccesses.size(),
       PlayerTickDependencies, 1, OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE,
       callbacks->player_tick, callbacks->player_context},
      {WorldTimeTickJobId, WorldTimeAccesses.data(), WorldTimeAccesses.size(),
       WorldTimeDependencies, 1, OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE,
       callbacks->world_time_tick, callbacks->world_time_context}};

  return octaryn_native_schedule_runtime_execute(schedule_runtime, jobs,
                                                 std::size(jobs), report);
}

int octaryn_server_authority_tick_validate_report(
    const octaryn_native_schedule_runtime_report *report) {
  if (report == nullptr) {
    return -1;
  }

  return report->submitted_jobs == AuthorityTickJobCount &&
                 report->completed_jobs == AuthorityTickJobCount &&
                 report->worker_jobs == AuthorityTickJobCount &&
                 report->main_thread_jobs == 0 && report->execution_waves == 3 &&
                 report->failed_job_index == -1
             ? 0
             : -2;
}
