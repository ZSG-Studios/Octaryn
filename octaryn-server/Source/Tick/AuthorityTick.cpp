#include "AuthorityTick.h"

#include "octaryn_native_schedule_policy.h"

#include <array>
#include <iterator>

namespace {

constexpr const char *PlayerTickJobId = "server.player.tick";
constexpr const char *WorldTimeTickJobId = "server.world_time.advance";
constexpr const char *WorldTimeDependencies[] = {PlayerTickJobId};

constexpr std::array<octaryn_native_schedule_resource_access, 1>
    PlayerTickAccesses = {
        {{"server.player.state", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}}};
constexpr std::array<octaryn_native_schedule_resource_access, 1>
    WorldTimeAccesses = {
        {{"server.world_time.clock", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}}};

bool valid_callbacks(const octaryn_server_authority_tick_callbacks *callbacks) {
  return callbacks != nullptr && callbacks->player_tick != nullptr &&
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
      {PlayerTickJobId, PlayerTickAccesses.data(), PlayerTickAccesses.size(),
       nullptr, 0, OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE,
       callbacks->player_tick, callbacks->player_context},
      {WorldTimeTickJobId, WorldTimeAccesses.data(), WorldTimeAccesses.size(),
       WorldTimeDependencies, 1, OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE,
       callbacks->world_time_tick, callbacks->world_time_context}};

  return octaryn_native_schedule_runtime_execute(schedule_runtime, jobs,
                                                 std::size(jobs), report);
}
