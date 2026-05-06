#include "AuthorityTick.h"
#include "octaryn_native_schedule_runtime.h"

#include <atomic>
#include <cstdio>
#include <string_view>

namespace {

struct TickProbeState {
  std::atomic<int> order{0};
  std::atomic<int> command_drains{0};
  std::atomic<int> player_ticks{0};
  std::atomic<int> world_time_ticks{0};
};

bool expect_equal(std::string_view label, int actual, int expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %d, got %d\n",
               static_cast<int>(label.size()), label.data(), expected, actual);
  return false;
}

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

int record_command_drain(void *context) {
  auto *state = static_cast<TickProbeState *>(context);
  state->command_drains.fetch_add(1, std::memory_order_acq_rel);
  int expected = 0;
  return state->order.compare_exchange_strong(expected, 1,
                                              std::memory_order_acq_rel)
             ? 0
             : -2;
}

int record_player_tick(void *context) {
  auto *state = static_cast<TickProbeState *>(context);
  state->player_ticks.fetch_add(1, std::memory_order_acq_rel);
  const int previous = state->order.load(std::memory_order_acquire);
  if (previous != 1) {
    return -2;
  }

  state->order.store(2, std::memory_order_release);
  return 0;
}

int record_world_time_tick(void *context) {
  auto *state = static_cast<TickProbeState *>(context);
  state->world_time_ticks.fetch_add(1, std::memory_order_acq_rel);
  const int previous = state->order.load(std::memory_order_acquire);
  if (previous != 2) {
    return -3;
  }

  state->order.store(3, std::memory_order_release);
  return 0;
}

int fail_command_drain(void *context) {
  auto *state = static_cast<TickProbeState *>(context);
  state->command_drains.fetch_add(1, std::memory_order_acq_rel);
  return -6;
}

int fail_player_tick(void *context) {
  auto *state = static_cast<TickProbeState *>(context);
  state->player_ticks.fetch_add(1, std::memory_order_acq_rel);
  return -7;
}

bool validate_authority_tick_order() {
  void *runtime = octaryn_native_schedule_runtime_create(12, 0);
  bool ok = expect_true("authority tick runtime creates", runtime != nullptr);
  if (runtime == nullptr) {
    return false;
  }

  TickProbeState state;
  const octaryn_server_authority_tick_callbacks callbacks = {
      record_command_drain,   &state, record_player_tick, &state,
      record_world_time_tick, &state};
  octaryn_native_schedule_runtime_report report = {};
  ok &= expect_equal(
      "authority tick execute",
      octaryn_server_authority_tick_execute(runtime, &callbacks, &report), 0);
  ok &= expect_equal("authority tick submitted jobs",
                     static_cast<int>(report.submitted_jobs), 3);
  ok &= expect_equal("authority tick completed jobs",
                     static_cast<int>(report.completed_jobs), 3);
  ok &= expect_equal("authority tick worker jobs",
                     static_cast<int>(report.worker_jobs), 3);
  ok &= expect_equal("authority tick main thread jobs",
                     static_cast<int>(report.main_thread_jobs), 0);
  ok &= expect_equal("authority tick dependency waves",
                     static_cast<int>(report.execution_waves), 3);
  ok &= expect_equal("authority tick failed job", report.failed_job_index, -1);
  ok &= expect_equal("authority tick command callback",
                     state.command_drains.load(std::memory_order_acquire), 1);
  ok &= expect_equal("authority tick player callback",
                     state.player_ticks.load(std::memory_order_acquire), 1);
  ok &= expect_equal("authority tick world time callback",
                     state.world_time_ticks.load(std::memory_order_acquire), 1);
  ok &= expect_equal("authority tick final order",
                     state.order.load(std::memory_order_acquire), 3);

  octaryn_native_schedule_runtime_destroy(runtime);
  return ok;
}

bool validate_authority_tick_failure() {
  void *runtime = octaryn_native_schedule_runtime_create(12, 0);
  bool ok =
      expect_true("authority tick failure runtime creates", runtime != nullptr);
  if (runtime == nullptr) {
    return false;
  }

  TickProbeState state;
  const octaryn_server_authority_tick_callbacks callbacks = {
      record_command_drain,   &state, fail_player_tick, &state,
      record_world_time_tick, &state};
  octaryn_native_schedule_runtime_report report = {};
  ok &= expect_equal(
      "authority tick returns callback failure",
      octaryn_server_authority_tick_execute(runtime, &callbacks, &report), -7);
  ok &=
      expect_equal("authority tick failure index", report.failed_job_index, 1);
  ok &= expect_equal("authority tick failure runs command drain",
                     state.command_drains.load(std::memory_order_acquire), 1);
  ok &= expect_equal("authority tick failure stops world time",
                     state.world_time_ticks.load(std::memory_order_acquire), 0);

  octaryn_native_schedule_runtime_destroy(runtime);
  return ok;
}

bool validate_authority_tick_command_failure() {
  void *runtime = octaryn_native_schedule_runtime_create(12, 0);
  bool ok = expect_true("authority tick command failure runtime creates",
                        runtime != nullptr);
  if (runtime == nullptr) {
    return false;
  }

  TickProbeState state;
  const octaryn_server_authority_tick_callbacks callbacks = {
      fail_command_drain,     &state, record_player_tick, &state,
      record_world_time_tick, &state};
  octaryn_native_schedule_runtime_report report = {};
  ok &= expect_equal(
      "authority tick returns command failure",
      octaryn_server_authority_tick_execute(runtime, &callbacks, &report), -6);
  ok &= expect_equal("authority tick command failure index",
                     report.failed_job_index, 0);
  ok &= expect_equal("authority tick command failure stops player",
                     state.player_ticks.load(std::memory_order_acquire), 0);
  ok &= expect_equal("authority tick command failure stops world time",
                     state.world_time_ticks.load(std::memory_order_acquire), 0);

  octaryn_native_schedule_runtime_destroy(runtime);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_authority_tick_order();
  ok &= validate_authority_tick_failure();
  ok &= validate_authority_tick_command_failure();

  if (!ok) {
    return 1;
  }

  std::puts("server authority tick probe passed");
  return 0;
}
