#include "ChunkMeshPlan.h"
#include "octaryn_native_schedule_policy.h"
#include "octaryn_native_worker_policy.h"

#include <atomic>
#include <bit>
#include <cstdio>
#include <iterator>
#include <string>
#include <string_view>
#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>

namespace {

bool expect_equal(std::string_view label, size_t actual, size_t expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected %zu, got %zu\n",
               static_cast<int>(label.size()), label.data(), expected, actual);
  return false;
}

bool expect_equal(std::string_view label, int32_t actual, int32_t expected) {
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

bool expect_action(std::string_view label, chunk_mesh_plan_action actual,
                   chunk_mesh_plan_action expected) {
  if (actual == expected) {
    return true;
  }

  std::fprintf(stderr, "%.*s: unexpected action\n",
               static_cast<int>(label.size()), label.data());
  return false;
}

chunk_view make_view(int32_t origin_x, int32_t origin_z, int32_t width) {
  return {origin_x, origin_z, width};
}

bool validate_initial_plan() {
  const chunk_view previous = make_view(0, 0, 0);
  const chunk_view current = make_view(0, 0, 4);
  const chunk_mesh_plan plan = build_chunk_mesh_plan(
      previous, current, chunk_mesh_plan_default_options(current));

  bool ok = true;
  ok &=
      expect_equal("initial active columns", plan.summary.active_columns, 16u);
  ok &=
      expect_equal("initial loaded columns", plan.summary.loaded_columns, 16u);
  ok &= expect_equal("initial preserved columns",
                     plan.summary.preserved_columns, 0u);
  ok &= expect_equal("initial unloaded columns", plan.summary.unloaded_columns,
                     0u);
  ok &= expect_equal("initial scheduled urgent jobs",
                     plan.summary.scheduled_urgent_jobs, 2u);
  ok &= expect_equal("initial scheduled regular jobs",
                     plan.summary.scheduled_regular_jobs, 0u);
  ok &= expect_equal("initial entry count", plan.entries.size(), 16u);
  ok &= expect_action("initial first action", plan.entries.front().action,
                      chunk_mesh_plan_action::build);
  ok &= expect_equal("initial first chunk x", plan.entries.front().chunk_x, 2);
  ok &= expect_equal("initial first chunk z", plan.entries.front().chunk_z, 2);
  return ok;
}

bool validate_shift_plan() {
  const chunk_view previous = make_view(0, 0, 4);
  const chunk_view current = make_view(1, 0, 4);
  const chunk_mesh_plan plan = build_chunk_mesh_plan(
      previous, current, chunk_mesh_plan_default_options(current));

  bool ok = true;
  ok &= expect_equal("shift loaded columns", plan.summary.loaded_columns, 4u);
  ok &= expect_equal("shift preserved columns", plan.summary.preserved_columns,
                     12u);
  ok &=
      expect_equal("shift unloaded columns", plan.summary.unloaded_columns, 4u);
  ok &= expect_equal("shift clear jobs", plan.summary.clear_jobs, 4u);
  ok &= expect_equal("shift entry count", plan.entries.size(), 20u);
  ok &= expect_action("shift first action", plan.entries.front().action,
                      chunk_mesh_plan_action::build);
  ok &= expect_equal("shift first loaded chunk x", plan.entries.front().chunk_x,
                     4);
  return ok;
}

bool validate_reset_plan() {
  const chunk_view previous = make_view(-1, -1, 2);
  const chunk_view current = make_view(0, 0, 0);
  const chunk_mesh_plan plan = build_chunk_mesh_plan(
      previous, current, chunk_mesh_plan_default_options(current));

  bool ok = true;
  ok &= expect_equal("reset active columns", plan.summary.active_columns, 0u);
  ok &= expect_equal("reset loaded columns", plan.summary.loaded_columns, 0u);
  ok &=
      expect_equal("reset unloaded columns", plan.summary.unloaded_columns, 4u);
  ok &= expect_equal("reset clear jobs", plan.summary.clear_jobs, 4u);
  ok &= expect_equal("reset entry count", plan.entries.size(), 4u);
  ok &= expect_action("reset first action", plan.entries.front().action,
                      chunk_mesh_plan_action::clear);
  return ok;
}

bool validate_taskflow_plan_execution() {
  const chunk_view previous = make_view(-1, -1, 5);
  const chunk_view current = make_view(1, 1, 5);
  const chunk_mesh_plan plan = build_chunk_mesh_plan(
      previous, current, chunk_mesh_plan_default_options(current));
  const int workers = octaryn_native_worker_policy_maximum_workers(8, 0);
  tf::Executor executor(static_cast<unsigned>(workers));
  tf::Taskflow taskflow;

  std::atomic<size_t> loaded{0u};
  std::atomic<size_t> preserved{0u};
  std::atomic<size_t> unloaded{0u};
  std::atomic<size_t> urgent{0u};
  taskflow.for_each(plan.entries.begin(), plan.entries.end(),
                    [&](const chunk_mesh_plan_entry &entry) {
                      switch (entry.action) {
                      case chunk_mesh_plan_action::build:
                        loaded.fetch_add(1u, std::memory_order_acq_rel);
                        if (entry.urgent) {
                          urgent.fetch_add(1u, std::memory_order_acq_rel);
                        }
                        break;
                      case chunk_mesh_plan_action::preserve:
                        preserved.fetch_add(1u, std::memory_order_acq_rel);
                        break;
                      case chunk_mesh_plan_action::clear:
                        unloaded.fetch_add(1u, std::memory_order_acq_rel);
                        break;
                      }
                    });

  executor.run(taskflow).wait();

  bool ok = true;
  ok &=
      expect_equal("taskflow worker policy", static_cast<size_t>(workers), 6u);
  ok &= expect_equal("taskflow loaded count",
                     loaded.load(std::memory_order_acquire),
                     plan.summary.loaded_columns);
  ok &= expect_equal("taskflow preserved count",
                     preserved.load(std::memory_order_acquire),
                     plan.summary.preserved_columns);
  ok &= expect_equal("taskflow unloaded count",
                     unloaded.load(std::memory_order_acquire),
                     plan.summary.unloaded_columns);
  ok &= expect_equal("taskflow urgent count",
                     urgent.load(std::memory_order_acquire),
                     plan.summary.urgent_jobs);
  return ok;
}

bool validate_render_distance_job_routing() {
  const chunk_view previous = make_view(0, 0, 0);
  const chunk_view current = make_view(-32, -32, 65);
  const chunk_mesh_plan plan = build_chunk_mesh_plan(
      previous, current, chunk_mesh_plan_default_options(current));
  const int workers = octaryn_native_worker_policy_maximum_workers(16, 0);

  bool ok = true;
  ok &=
      expect_equal("render distance active columns",
                   plan.summary.active_columns, 4225u);
  ok &=
      expect_equal("render distance loaded columns",
                   plan.summary.loaded_columns, 4225u);
  ok &= expect_equal("render distance urgent jobs",
                     plan.summary.urgent_jobs, 121u);
  ok &= expect_equal("render distance regular jobs",
                     plan.summary.regular_jobs, 4104u);
  ok &= expect_equal("render distance scheduled urgent jobs",
                     plan.summary.scheduled_urgent_jobs, 2u);
  ok &= expect_equal("render distance scheduled regular jobs",
                     plan.summary.scheduled_regular_jobs, 1u);
  ok &= expect_equal("render distance native workers",
                     static_cast<size_t>(workers), 13u);

  const std::string block_resource = "chunk.0.0.blocks";
  const std::string mesh_resource = "chunk.0.0.mesh";
  const std::string other_block_resource = "chunk.1.0.blocks";
  const std::string other_mesh_resource = "chunk.1.0.mesh";
  const std::string upload_resource = "gpu.mesh_upload";

  const octaryn_native_schedule_resource_access build_accesses[] = {
      {block_resource.c_str(), OCTARYN_NATIVE_SCHEDULE_ACCESS_READ},
      {mesh_resource.c_str(), OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}};
  const octaryn_native_schedule_resource_access upload_accesses[] = {
      {mesh_resource.c_str(), OCTARYN_NATIVE_SCHEDULE_ACCESS_READ},
      {upload_resource.c_str(), OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}};
  const octaryn_native_schedule_resource_access other_build_accesses[] = {
      {other_block_resource.c_str(), OCTARYN_NATIVE_SCHEDULE_ACCESS_READ},
      {other_mesh_resource.c_str(), OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}};

  const octaryn_native_schedule_job build_job = {
      "mesh_build_0_0",
      build_accesses,
      std::size(build_accesses),
      OCTARYN_NATIVE_SCHEDULE_JOB_NONE};
  const octaryn_native_schedule_job upload_job = {
      "mesh_upload_0_0",
      upload_accesses,
      std::size(upload_accesses),
      OCTARYN_NATIVE_SCHEDULE_JOB_MAIN_THREAD};
  const octaryn_native_schedule_job other_build_job = {
      "mesh_build_1_0",
      other_build_accesses,
      std::size(other_build_accesses),
      OCTARYN_NATIVE_SCHEDULE_JOB_NONE};

  octaryn_native_schedule_conflict conflict = {};
  ok &= expect_false("build/upload handoff cannot overlap mesh resource",
                     octaryn_native_schedule_jobs_can_run_concurrently(
                         &build_job, &upload_job, &conflict) != 0);
  ok &= expect_equal("build/upload conflict left index",
                     static_cast<int>(conflict.left_access_index), 1);
  ok &= expect_equal("build/upload conflict right index",
                     static_cast<int>(conflict.right_access_index), 0);
  ok &= expect_true("build/upload conflict captures mesh resource",
                    std::string_view(conflict.resource_id) == mesh_resource);
  ok &= expect_true("independent mesh builds can use workers",
                    octaryn_native_schedule_jobs_can_run_concurrently(
                        &build_job, &other_build_job, &conflict) != 0);
  ok &= expect_false("main-thread upload handoff is nonblocking",
                     octaryn_native_schedule_job_blocks_main_thread(
                         &upload_job) != 0);
  return ok;
}

} // namespace

int main() {
  bool ok = true;
  ok &= validate_initial_plan();
  ok &= validate_shift_plan();
  ok &= validate_reset_plan();
  ok &= validate_taskflow_plan_execution();
  ok &= validate_render_distance_job_routing();

  if (!ok) {
    return 1;
  }

  std::puts("client chunk mesh plan probe passed");
  return 0;
}
