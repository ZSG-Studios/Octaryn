#include "FluidSimulation.h"
#include "FluidScheduler.h"
#include "BlockChangeQueue.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace {
using namespace octaryn::server::world::blocks;
struct Simulation {
  explicit Simulation(FluidRules rules) : scheduler(std::move(rules)) {}
  FluidScheduler scheduler;
  double elapsed_ms{};
};
static_assert(sizeof(octaryn_server_fluid_config) == 72);
static_assert(sizeof(octaryn_server_fluid_tick_report) == 48);
}

extern "C" {
void *octaryn_server_fluid_create(const octaryn_server_fluid_config *config) {
  if (!config || config->version != 1 || config->size != sizeof(*config) ||
      config->replaceable_count > 65535 || config->solid_count > 65535 ||
      (config->replaceable_count && !config->replaceable) ||
      (config->solid_count && !config->solid)) return nullptr;
  try {
    FluidRules rules;
    std::copy_n(config->water, 8, rules.water.begin());
    std::copy_n(config->lava, 8, rules.lava.begin());
    rules.stone = config->stone;
    if (config->replaceable_count)
      rules.replaceable.assign(config->replaceable, config->replaceable + config->replaceable_count);
    if (config->solid_count)
      rules.solid.assign(config->solid, config->solid + config->solid_count);
    if (!validate_fluid_rules(rules)) return nullptr;
    return new Simulation(std::move(rules));
  } catch (...) { return nullptr; }
}

void octaryn_server_fluid_destroy(void *simulation) {
  delete static_cast<Simulation *>(simulation);
}

int32_t octaryn_server_fluid_set_region(void *simulation, int32_t center_x,
    int32_t center_z, uint32_t radius) {
  auto *state = static_cast<Simulation *>(simulation);
  return state && state->scheduler.set_region({center_x, center_z, radius}) ? 0 : -1;
}

int32_t octaryn_server_fluid_notify(void *simulation,
    const octaryn_server_block_edit *changes, uint32_t count) {
  auto *state = static_cast<Simulation *>(simulation);
  if (!state || (count && !changes)) return -1;
  const auto now = static_cast<uint64_t>(state->elapsed_ms);
  for (uint32_t i = 0; i < count; ++i) {
    const auto &edit = changes[i];
    state->scheduler.notify_change({edit.position.x, edit.position.y, edit.position.z},
        edit.block, edit.block, now);
  }
  return 0;
}

int32_t octaryn_server_fluid_tick(void *simulation, void *store, void *change_queue,
    double delta_seconds, octaryn_server_generated_block_fn generated_block,
    octaryn_server_block_known_fn is_known_block,
    octaryn_server_block_can_apply_fn can_apply_edit,
    octaryn_server_block_can_stay_supported_fn can_stay_supported,
    void *context, octaryn_server_fluid_tick_report *report) {
  auto *state = static_cast<Simulation *>(simulation);
  auto *blocks = static_cast<BlockStore *>(store);
  if (!state || !blocks || !report || !std::isfinite(delta_seconds) || delta_seconds < 0)
    return -1;
  state->elapsed_ms += std::min(delta_seconds, 0.25) * 1000.0;
  const auto now = static_cast<uint64_t>(state->elapsed_ms);
  const auto policy = policy_from_abi(generated_block, is_known_block, can_apply_edit,
      can_stay_supported, context);
  auto *queue = static_cast<BlockChangeQueue *>(change_queue);
  uint32_t changed = 0, deferred = 0;
  const FluidRead read = [&](const BlockPosition &position, uint16_t &value) {
    value = get_effective_block(*blocks, position, policy);
    return true;
  };
  const FluidApply apply = [&](const BlockPosition &position, uint16_t expected, uint16_t next) {
    if (get_effective_block(*blocks, position, policy) != expected) return FluidApplyResult::Retry;
    const auto result = apply_block_edit_and_enqueue(*blocks, queue, {position, next}, policy);
    if (result.deferred || !result.result.applied) {
      ++deferred;
      return FluidApplyResult::Retry;
    }
    changed += static_cast<uint32_t>(result.changes.size());
    for (const auto &edit : result.changes)
      state->scheduler.notify_change(edit.position,
          edit.position == position ? expected : AirBlock, edit.block, now);
    return result.result.changed ? FluidApplyResult::Applied : FluidApplyResult::Unchanged;
  };
  const auto tick = state->scheduler.tick(now, read, apply);
  *report = {1, sizeof(*report), tick.evaluations, tick.applies, changed,
      tick.pending, tick.reads, tick.repair_samples, deferred,
      tick.budget_exhausted ? 1u : 0u, now};
  return 0;
}
}
