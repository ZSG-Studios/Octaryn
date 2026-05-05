#include "octaryn_native_schedule_policy.h"
#include "octaryn_native_worker_policy.h"

#include <atomic>
#include <bit>
#include <cstdio>
#include <string_view>
#include <thread>
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

bool expect_false(std::string_view label, bool value) {
  if (!value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected false\n", static_cast<int>(label.size()),
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

bool validate_schedule_policy() {
  bool ok = true;

  const octaryn_native_schedule_resource_access chunk_read = {
      "chunk.0.0", OCTARYN_NATIVE_SCHEDULE_ACCESS_READ};
  const octaryn_native_schedule_resource_access chunk_read_again = {
      "chunk.0.0", OCTARYN_NATIVE_SCHEDULE_ACCESS_READ};
  const octaryn_native_schedule_resource_access chunk_write = {
      "chunk.0.0", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE};
  const octaryn_native_schedule_resource_access neighbor_write = {
      "chunk.1.0", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE};

  ok &= expect_false("read-only accesses do not conflict",
                     octaryn_native_schedule_accesses_conflict(
                         &chunk_read, &chunk_read_again) != 0);
  ok &= expect_true("read/write accesses conflict",
                    octaryn_native_schedule_accesses_conflict(
                        &chunk_read, &chunk_write) != 0);
  ok &= expect_true("write/write accesses conflict",
                    octaryn_native_schedule_accesses_conflict(
                        &chunk_write, &chunk_write) != 0);
  ok &= expect_false("different resources do not conflict",
                     octaryn_native_schedule_accesses_conflict(
                         &chunk_read, &neighbor_write) != 0);
  ok &= expect_false("null access does not conflict",
                     octaryn_native_schedule_accesses_conflict(
                         nullptr, &chunk_write) != 0);

  const octaryn_native_schedule_resource_access mesh_writer_accesses[] = {
      {"chunk.0.0.blocks", OCTARYN_NATIVE_SCHEDULE_ACCESS_READ},
      {"chunk.0.0.mesh", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}};
  const octaryn_native_schedule_resource_access mesh_reader_accesses[] = {
      {"chunk.0.0.mesh", OCTARYN_NATIVE_SCHEDULE_ACCESS_READ}};
  const octaryn_native_schedule_resource_access neighbor_mesh_accesses[] = {
      {"chunk.1.0.blocks", OCTARYN_NATIVE_SCHEDULE_ACCESS_READ},
      {"chunk.1.0.mesh", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}};

  const octaryn_native_schedule_job mesh_writer = {
      "mesh_writer", mesh_writer_accesses, 2, OCTARYN_NATIVE_SCHEDULE_JOB_NONE};
  const octaryn_native_schedule_job mesh_reader = {
      "mesh_reader", mesh_reader_accesses, 1, OCTARYN_NATIVE_SCHEDULE_JOB_NONE};
  const octaryn_native_schedule_job neighbor_mesh_writer = {
      "neighbor_mesh_writer",
      neighbor_mesh_accesses,
      2,
      OCTARYN_NATIVE_SCHEDULE_JOB_NONE};

  octaryn_native_schedule_conflict conflict = {};
  ok &= expect_true("jobs report write/read conflict",
                    octaryn_native_schedule_jobs_conflict(
                        &mesh_writer, &mesh_reader, &conflict) != 0);
  ok &= expect_equal("conflict left index", static_cast<int>(conflict.left_access_index), 1);
  ok &= expect_equal("conflict right index", static_cast<int>(conflict.right_access_index), 0);
  ok &= expect_true("conflict captures resource id",
                    std::string_view(conflict.resource_id) == "chunk.0.0.mesh");

  ok &= expect_false("independent jobs do not conflict",
                     octaryn_native_schedule_jobs_conflict(
                         &mesh_writer, &neighbor_mesh_writer, &conflict) != 0);
  ok &= expect_equal("non-conflict clears previous conflict", conflict.has_conflict, 0);

  ok &= expect_false("conflicting jobs cannot run concurrently",
                     octaryn_native_schedule_jobs_can_run_concurrently(
                         &mesh_writer, &mesh_reader, &conflict) != 0);
  ok &= expect_true("independent jobs can run concurrently",
                    octaryn_native_schedule_jobs_can_run_concurrently(
                        &mesh_writer, &neighbor_mesh_writer, &conflict) != 0);

  const octaryn_native_schedule_job main_thread_blocking_job = {
      "present_wait",
      nullptr,
      0,
      OCTARYN_NATIVE_SCHEDULE_JOB_MAIN_THREAD |
          OCTARYN_NATIVE_SCHEDULE_JOB_BLOCKING};
  const octaryn_native_schedule_job main_thread_handoff_job = {
      "present_handoff",
      nullptr,
      0,
      OCTARYN_NATIVE_SCHEDULE_JOB_MAIN_THREAD};
  const octaryn_native_schedule_job worker_blocking_job = {
      "worker_wait", nullptr, 0, OCTARYN_NATIVE_SCHEDULE_JOB_BLOCKING};

  ok &= expect_true("main-thread blocking job is rejected",
                    octaryn_native_schedule_job_blocks_main_thread(
                        &main_thread_blocking_job) != 0);
  ok &= expect_false("main-thread handoff without blocking is allowed",
                     octaryn_native_schedule_job_blocks_main_thread(
                         &main_thread_handoff_job) != 0);
  ok &= expect_false("worker blocking flag does not block main thread",
                     octaryn_native_schedule_job_blocks_main_thread(
                         &worker_blocking_job) != 0);
  ok &= expect_false("concurrency rejects main-thread blocking job",
                     octaryn_native_schedule_jobs_can_run_concurrently(
                         &main_thread_blocking_job, &neighbor_mesh_writer,
                         &conflict) != 0);

  return ok;
}

bool validate_command_write_scope() {
  bool ok = true;

  ok &= expect_equal("command write starts inactive",
                     octaryn_native_command_write_scope_is_active(), 0);
  ok &= expect_equal("command write starts at zero depth",
                     static_cast<int>(octaryn_native_command_write_scope_depth()), 0);
  ok &= expect_equal("command write enter returns depth one",
                     static_cast<int>(octaryn_native_command_write_scope_enter()), 1);
  ok &= expect_equal("command write active after enter",
                     octaryn_native_command_write_scope_is_active(), 1);
  ok &= expect_equal("command write nested enter returns depth two",
                     static_cast<int>(octaryn_native_command_write_scope_enter()), 2);
  ok &= expect_equal("command write nested depth visible",
                     static_cast<int>(octaryn_native_command_write_scope_depth()), 2);

  std::atomic<int> worker_active = -1;
  std::atomic<int> worker_depth = -1;
  std::thread worker([&worker_active, &worker_depth] {
    worker_active.store(octaryn_native_command_write_scope_is_active(),
                        std::memory_order_release);
    worker_depth.store(static_cast<int>(octaryn_native_command_write_scope_depth()),
                       std::memory_order_release);
  });
  worker.join();

  ok &= expect_equal("command write scope is thread local",
                     worker_active.load(std::memory_order_acquire), 0);
  ok &= expect_equal("command write depth is thread local",
                     worker_depth.load(std::memory_order_acquire), 0);
  ok &= expect_equal("command write first exit returns depth one",
                     static_cast<int>(octaryn_native_command_write_scope_exit()), 1);
  ok &= expect_equal("command write second exit returns depth zero",
                     static_cast<int>(octaryn_native_command_write_scope_exit()), 0);
  ok &= expect_equal("command write inactive after exits",
                     octaryn_native_command_write_scope_is_active(), 0);
  ok &= expect_equal("command write underflow is clamped",
                     static_cast<int>(octaryn_native_command_write_scope_exit()), 0);

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
  ok &= validate_schedule_policy();
  ok &= validate_command_write_scope();
  ok &= validate_taskflow_dependencies();

  if (!ok) {
    return 1;
  }

  std::puts("native jobs probe passed");
  return 0;
}
