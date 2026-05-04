#include "octaryn_native_worker_policy.h"

#include <atomic>
#include <bit>
#include <cstdio>
#include <string_view>
#include <taskflow/taskflow.hpp>

namespace {

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

bool validate_worker_policy() {
  bool ok = true;

  ok &= expect_equal("minimum workers on one core",
                     octaryn_native_worker_policy_minimum_workers(1), 2);
  ok &= expect_equal("maximum workers on one core",
                     octaryn_native_worker_policy_maximum_workers(1, 0), 2);
  ok &= expect_equal("maximum workers on six cores",
                     octaryn_native_worker_policy_maximum_workers(6, 0), 5);
  ok &= expect_equal("maximum workers on twelve cores",
                     octaryn_native_worker_policy_maximum_workers(12, 0), 10);
  ok &= expect_equal("configured limit clamps low",
                     octaryn_native_worker_policy_maximum_workers(12, 1), 2);
  ok &= expect_equal("configured limit clamps high",
                     octaryn_native_worker_policy_maximum_workers(12, 64), 10);
  ok &= expect_equal("manual worker mode",
                     octaryn_native_worker_mode_from_string(
                         "manual", OCTARYN_NATIVE_WORKER_MODE_AUTO),
                     OCTARYN_NATIVE_WORKER_MODE_MANUAL);
  ok &= expect_equal("auto worker mode fallback",
                     octaryn_native_worker_mode_from_string(
                         "invalid", OCTARYN_NATIVE_WORKER_MODE_MANUAL),
                     OCTARYN_NATIVE_WORKER_MODE_MANUAL);

  const auto idle_sample =
      octaryn_native_worker_policy_sample_create(12, 0, 6, 0, 0, 0, 0);
  ok &= expect_equal("idle target preserves current workers",
                     idle_sample.target_workers, 6);

  const auto backlog_sample =
      octaryn_native_worker_policy_sample_create(12, 0, 6, 0, 3, 0, 0);
  ok &= expect_equal("backlog target uses maximum workers",
                     backlog_sample.target_workers, 10);

  const auto render_pressure_sample =
      octaryn_native_worker_policy_sample_create(12, 0, 6, 80, 3, 0, 0);
  ok &= expect_equal("render pressure reserves worker capacity",
                     render_pressure_sample.target_workers, 8);

  const auto upload_pressure_sample =
      octaryn_native_worker_policy_sample_create(12, 0, 6, 0, 3, 80, 0);
  ok &= expect_equal("upload pressure reserves worker capacity",
                     upload_pressure_sample.target_workers, 8);

  return ok;
}

bool validate_taskflow_dependencies() {
  tf::Taskflow taskflow;
  tf::Executor executor(2);

  std::atomic<int> first_done = 0;
  std::atomic<int> branch_count = 0;
  std::atomic<int> final_done = 0;

  auto first = taskflow.emplace(
      [&first_done] { first_done.store(1, std::memory_order_release); });

  auto branch_a = taskflow.emplace([&first_done, &branch_count] {
    if (first_done.load(std::memory_order_acquire) == 1) {
      branch_count.fetch_add(1, std::memory_order_acq_rel);
    }
  });

  auto branch_b = taskflow.emplace([&first_done, &branch_count] {
    if (first_done.load(std::memory_order_acquire) == 1) {
      branch_count.fetch_add(1, std::memory_order_acq_rel);
    }
  });

  auto final = taskflow.emplace([&branch_count, &final_done] {
    if (branch_count.load(std::memory_order_acquire) == 2) {
      final_done.store(1, std::memory_order_release);
    }
  });

  first.precede(branch_a, branch_b);
  branch_a.precede(final);
  branch_b.precede(final);

  executor.run(taskflow).wait();

  bool ok = true;
  ok &= expect_equal("taskflow first task ran",
                     first_done.load(std::memory_order_acquire), 1);
  ok &= expect_equal("taskflow branches ran after dependency",
                     branch_count.load(std::memory_order_acquire), 2);
  ok &= expect_equal("taskflow barrier ran after branches",
                     final_done.load(std::memory_order_acquire), 1);
  ok &= expect_true("taskflow dependency graph has four tasks",
                    taskflow.num_tasks() == 4);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_worker_policy();
  ok &= validate_taskflow_dependencies();

  if (!ok) {
    return 1;
  }

  std::puts("native jobs probe passed");
  return 0;
}
