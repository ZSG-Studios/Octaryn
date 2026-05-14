#include "WorldMeshRuntime.h"
#include "FrameProfile.h"
#include "Log.h"
#include "WorldMeshBatchBudget.h"
#include "WorldMeshRetainedColumns.h"
#include "octaryn_native_schedule_runtime.h"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <iterator>
#include <thread>
namespace octaryn_client_app {
namespace {
struct scheduled_world_mesh_update {
  SDL_GPUDevice *gpu_device = nullptr;
  world_mesh_upload_frame *visible_frame = nullptr;
  world_mesh_gpu_buffers *mesh_buffers = nullptr;
  world_mesh_upload_frame update_frame{};
  uint64_t frame_index = 0u;
  const char *source = "";
  float build_ms = 0.0f;
  float upload_ms = 0.0f;
  int result = 0;
};
struct server_stream_build_context {
  scheduled_world_mesh_update *update = nullptr;
  const server_chunk_stream_file *stream = nullptr;
  const block_lookup *block_lookup = nullptr;
  const chunk_view *previous_chunk_view = nullptr;
  const std::vector<empty_world_dirty_column> *dirty_columns = nullptr;
  const std::vector<empty_world_retained_column> *retained_columns = nullptr;
  size_t first_plan_entry = 0u, max_plan_entries = 0u;
  empty_world_stream_mesh_batch_result *batch_result = nullptr;
};
struct empty_world_build_context {
  scheduled_world_mesh_update *update = nullptr;
  const chunk_view *current_view = nullptr;
  const chunk_view *previous_chunk_view = nullptr;
  const block_lookup *block_lookup = nullptr;
};
struct pending_server_stream_mesh_build {
  scheduled_world_mesh_update update{};
  server_chunk_stream_file stream{};
  block_lookup block_lookup{};
  chunk_view previous_chunk_view{};
  std::vector<empty_world_retained_column> retained_columns{};
  std::vector<empty_world_dirty_column> dirty_columns{};
  empty_world_stream_mesh_batch_result batch_result{};
  server_stream_build_context build_context{};
  octaryn_native_schedule_resource_access build_accesses[2]{};
  octaryn_native_schedule_runtime_job build_job{};
  void *task = nullptr;
  uint64_t submit_ticks = 0u, submit_frame = 0u;
};
int build_server_stream_mesh(void *context) {
  auto *build = static_cast<server_stream_build_context *>(context);
  const uint64_t start_ticks = SDL_GetTicksNS();
  build_empty_world_mesh_frame_from_stream_batch(
      *build->stream, *build->block_lookup, *build->previous_chunk_view,
      *build->dirty_columns, *build->retained_columns, build->first_plan_entry,
      build->max_plan_entries, build->update->update_frame, *build->batch_result);
  build->update->build_ms = frame_profile_elapsed_ms_since(start_ticks);
  return 0;
}
int build_empty_world_mesh(void *context) {
  auto *build = static_cast<empty_world_build_context *>(context);
  const uint64_t start_ticks = SDL_GetTicksNS();
  build_empty_world_mesh_frame(
      *build->current_view, *build->previous_chunk_view, *build->block_lookup,
      build->update->update_frame);
  build->update->build_ms = frame_profile_elapsed_ms_since(start_ticks);
  return 0;
}
int upload_scheduled_world_mesh(void *context) {
  auto *update = static_cast<scheduled_world_mesh_update *>(context);
  const uint64_t start_ticks = SDL_GetTicksNS();
  if (!apply_world_mesh_upload_update(
          update->gpu_device, *update->visible_frame, update->update_frame,
          *update->mesh_buffers, update->frame_index, update->source,
          update->result)) {
    update->upload_ms = frame_profile_elapsed_ms_since(start_ticks);
    return update->result;
  }
  update->upload_ms = frame_profile_elapsed_ms_since(start_ticks);
  return 0;
}
bool execute_scheduled_update(world_mesh_runtime &runtime,
                              scheduled_world_mesh_update &update,
                              octaryn_native_schedule_execute_fn build_execute,
                              void *build_context, int &result) {
  const octaryn_native_schedule_resource_access build_accesses[] = {
      {"chunk_stream.window", OCTARYN_NATIVE_SCHEDULE_ACCESS_READ},
      {"world_mesh.update_frame", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}};
  const octaryn_native_schedule_resource_access upload_accesses[] = {
      {"world_mesh.update_frame", OCTARYN_NATIVE_SCHEDULE_ACCESS_READ},
      {"gpu.world_mesh_upload", OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE}};
  const char *upload_after[] = {"world_mesh_build"};
  const octaryn_native_schedule_runtime_job jobs[] = {
      {"world_mesh_build", build_accesses, std::size(build_accesses), nullptr,
       0u, OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE, build_execute,
       build_context},
      {"world_mesh_upload", upload_accesses, std::size(upload_accesses),
       upload_after, std::size(upload_after),
       OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_MAIN_THREAD,
       upload_scheduled_world_mesh, &update}};
  const uint64_t start_ticks = SDL_GetTicksNS();
  octaryn_native_schedule_runtime_report report{};
  const int runtime_result = octaryn_native_schedule_runtime_execute(
      runtime.handle, jobs, std::size(jobs), &report);
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_native_schedule_runtime frame=%" PRIu64
                 " active=%d source=%s jobs=%zu completed=%zu worker_jobs=%zu"
                 " main_thread_jobs=%zu waves=%zu failed_job=%d elapsed_ms=%.3f"
                 " build_ms=%.3f upload_ms=%.3f chunks=%zu opaque_faces=%zu"
                 " sprite_vertices=%zu result=%d\n",
                 update.frame_index, runtime_result == 0 ? 1 : 0, update.source,
                 report.submitted_jobs, report.completed_jobs,
                 report.worker_jobs, report.main_thread_jobs,
                 report.execution_waves, report.failed_job_index,
                 frame_profile_elapsed_ms_since(start_ticks), update.build_ms,
                 update.upload_ms, update.update_frame.chunks.size(),
                 update.update_frame.opaque_faces.size(),
                 update.update_frame.sprite_vertices.size(), runtime_result);
    std::fflush(g_log);
  }
  if (runtime_result != 0) {
    result = runtime_result;
    return false;
  }
  return true;
}
pending_server_stream_mesh_build *pending_server_stream_build(
    world_mesh_runtime &runtime) {
  return static_cast<pending_server_stream_mesh_build *>(
      runtime.server_stream_pending);
}

bool same_batch_target(const server_stream_mesh_batch_state &batch,
                       const server_chunk_stream_file &stream,
                       const chunk_view &previous_view,
                       const chunk_view &target_view) {
  return batch.active && batch.epoch == stream.epoch &&
         same_chunk_view(batch.previous_view, previous_view) &&
         same_chunk_view(batch.target_view, target_view);
}

void start_server_stream_mesh_batch(server_stream_mesh_batch_state &batch,
                                    const server_chunk_stream_file &stream,
                                    const chunk_view &previous_view,
                                    const chunk_view &target_view) {
  batch.active = true;
  batch.epoch = stream.epoch;
  batch.previous_view = previous_view;
  batch.target_view = target_view;
  batch.next_plan_entry = 0u;
  batch.batch_index = 0u;
}

bool same_pending_target(const pending_server_stream_mesh_build &pending,
                         const server_chunk_stream_file &stream,
                         const chunk_view &previous_view,
                         const chunk_view &target_view) {
  return pending.stream.epoch == stream.epoch &&
         same_chunk_view(pending.previous_chunk_view, previous_view) &&
         same_chunk_view(chunk_view_from_server_stream(pending.stream),
                         target_view);
}
void destroy_pending_server_stream_build(world_mesh_runtime &runtime) {
  auto *pending = pending_server_stream_build(runtime);
  if (pending == nullptr) {
    return;
  }
  octaryn_native_schedule_runtime_task_destroy(pending->task);
  delete pending;
  runtime.server_stream_pending = nullptr;
}
void abandon_pending_server_stream_build(world_mesh_runtime &runtime) {
  auto *pending = pending_server_stream_build(runtime);
  if (pending == nullptr) {
    return;
  }
  runtime.abandoned_server_stream_builds.push_back(pending);
  runtime.server_stream_pending = nullptr;
}
void destroy_abandoned_server_stream_builds(world_mesh_runtime &runtime) {
  for (void *build : runtime.abandoned_server_stream_builds) {
    auto *pending = static_cast<pending_server_stream_mesh_build *>(build);
    octaryn_native_schedule_runtime_task_destroy(pending->task);
    delete pending;
  }
  runtime.abandoned_server_stream_builds.clear();
}
bool start_pending_server_stream_build(
    world_mesh_runtime &runtime, const server_chunk_stream_file &stream,
    const block_lookup &block_lookup, const chunk_view &previous_chunk_view,
    const std::vector<empty_world_dirty_column> &dirty_columns,
    const world_mesh_upload_frame &retained_frame,
    uint64_t frame_index, const char *source, int &result) {
  auto *pending = new pending_server_stream_mesh_build();
  pending->update.frame_index = frame_index;
  pending->update.source = source;
  pending->stream = stream;
  pending->block_lookup = block_lookup;
  pending->previous_chunk_view = previous_chunk_view;
  pending->retained_columns = retained_columns_from_frame(retained_frame);
  pending->dirty_columns = dirty_columns;
  pending->submit_ticks = SDL_GetTicksNS();
  pending->submit_frame = frame_index;
  pending->build_context = {&pending->update,
                            &pending->stream,
                            &pending->block_lookup,
                            &pending->previous_chunk_view,
                            &pending->dirty_columns,
                            &pending->retained_columns,
                            runtime.server_stream_batch.next_plan_entry,
                            server_stream_mesh_batch_budget(),
                            &pending->batch_result};
  pending->build_accesses[0] = {"chunk_stream.window",
                                OCTARYN_NATIVE_SCHEDULE_ACCESS_READ};
  pending->build_accesses[1] = {"world_mesh.update_frame",
                                OCTARYN_NATIVE_SCHEDULE_ACCESS_WRITE};
  pending->build_job = {"world_mesh_build",
                        pending->build_accesses,
                        std::size(pending->build_accesses),
                        nullptr,
                        0u,
                        OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_NONE,
                        build_server_stream_mesh,
                        &pending->build_context};
  pending->task = octaryn_native_schedule_runtime_submit_worker(
      runtime.handle, &pending->build_job, 1u);
  if (pending->task == nullptr) {
    delete pending;
    result = -7;
    return false;
  }
  runtime.server_stream_pending = pending;
  return true;
}

bool complete_pending_server_stream_build(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame,
    world_mesh_gpu_buffers &mesh_buffers,
    const server_chunk_stream_file &stream, const chunk_view &previous_view,
    const chunk_view &target_view, uint64_t frame_index, const char *source,
    bool &applied_pending, int &result) {
  applied_pending = false;
  auto *pending = pending_server_stream_build(runtime);
  if (pending == nullptr) {
    return true;
  }
  const int ready = octaryn_native_schedule_runtime_task_ready(pending->task);
  if (ready < 0) {
    result = ready;
    return false;
  }
  if (ready == 0) {
    runtime.server_stream_batch.active = true;
    return true;
  }

  octaryn_native_schedule_runtime_report report{};
  const int build_result =
      octaryn_native_schedule_runtime_task_result(pending->task, &report);
  octaryn_native_schedule_runtime_task_destroy(pending->task);
  pending->task = nullptr;
  if (build_result != 0) {
    result = build_result;
    delete pending;
    runtime.server_stream_pending = nullptr;
    return false;
  }

  const bool target_matches =
      same_pending_target(*pending, stream, previous_view, target_view);
  if (!target_matches) {
    delete pending;
    runtime.server_stream_pending = nullptr;
    runtime.server_stream_batch.active = false;
    return true;
  }

  pending->update.frame_index = frame_index;
  pending->update.source = source;
  const uint64_t upload_start_ticks = SDL_GetTicksNS();
  if (!apply_world_mesh_upload_update(
          gpu_device, visible_frame, pending->update.update_frame, mesh_buffers,
          frame_index, source, result)) {
    pending->update.upload_ms =
        frame_profile_elapsed_ms_since(upload_start_ticks);
    delete pending;
    runtime.server_stream_pending = nullptr;
    return false;
  }
  pending->update.upload_ms = frame_profile_elapsed_ms_since(upload_start_ticks);
  runtime.server_stream_batch.next_plan_entry = pending->batch_result.next_entry;
  ++runtime.server_stream_batch.batch_index;
  runtime.server_stream_batch.active = !pending->batch_result.complete;
  applied_pending = true;

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_native_schedule_runtime frame=%" PRIu64
                 " active=1 source=%s jobs=2 completed=%zu worker_jobs=%zu"
                 " main_thread_jobs=1 waves=%zu failed_job=%d elapsed_ms=%.3f"
                 " build_ms=%.3f upload_ms=%.3f chunks=%zu opaque_faces=%zu"
                 " sprite_vertices=%zu result=0 async=1 submit_frame=%" PRIu64
                 "\n",
                 frame_index, source, report.completed_jobs + 1u,
                 report.worker_jobs, report.execution_waves + 1u,
                 report.failed_job_index,
                 frame_profile_elapsed_ms_since(pending->submit_ticks),
                 pending->update.build_ms, pending->update.upload_ms,
                 pending->update.update_frame.chunks.size(),
                 pending->update.update_frame.opaque_faces.size(),
                 pending->update.update_frame.sprite_vertices.size(),
                 pending->submit_frame);
    std::fprintf(
        g_log,
        "live_server_stream_mesh_batch frame=%" PRIu64
        " active=1 source=%s epoch=%" PRIu64
        " batch=%zu first_entry=%zu next_entry=%zu processed=%zu"
        " remaining=%zu complete=%d build_columns=%zu clear_columns=%zu"
        " radius=%" PRIu32 " columns=%zu chunks=%zu opaque_faces=%zu"
        " build_ms=%.3f upload_ms=%.3f\n",
        frame_index, source, stream.epoch, runtime.server_stream_batch.batch_index,
        pending->batch_result.first_entry, pending->batch_result.next_entry,
        pending->batch_result.processed_entries,
        pending->batch_result.remaining_entries,
        pending->batch_result.complete ? 1 : 0,
        pending->batch_result.build_columns, pending->batch_result.clear_columns,
        stream.radius, stream.columns.size(),
        pending->update.update_frame.chunks.size(),
        pending->update.update_frame.opaque_faces.size(),
        pending->update.build_ms, pending->update.upload_ms);
    std::fflush(g_log);
  }

  delete pending;
  runtime.server_stream_pending = nullptr;
  return true;
}

} // namespace

bool world_mesh_runtime_start(world_mesh_runtime &runtime) {
  if (runtime.handle != nullptr) {
    return true;
  }

  const unsigned int cores = std::thread::hardware_concurrency();
  runtime.handle =
      octaryn_native_schedule_runtime_create(static_cast<int>(cores), 0);
  if (runtime.handle == nullptr) {
    log_line("live_native_schedule_runtime active=0 reason=create_failed");
    return false;
  }
  return true;
}

void world_mesh_runtime_stop(world_mesh_runtime &runtime) {
  destroy_pending_server_stream_build(runtime);
  destroy_abandoned_server_stream_builds(runtime);
  octaryn_native_schedule_runtime_destroy(runtime.handle);
  runtime.handle = nullptr;
}

bool run_server_stream_world_mesh_update(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame,
    world_mesh_gpu_buffers &mesh_buffers,
    const server_chunk_stream_file &stream, const block_lookup &block_lookup,
    const chunk_view &previous_chunk_view,
    const std::vector<empty_world_dirty_column> &dirty_columns,
    uint64_t frame_index, const char *source, int &result) {
  const chunk_view target_view = chunk_view_from_server_stream(stream);
  bool applied_pending = false;
  if (runtime.server_stream_pending != nullptr &&
      !complete_pending_server_stream_build(
          runtime, gpu_device, visible_frame, mesh_buffers, stream,
          previous_chunk_view, target_view, frame_index, source,
          applied_pending, result)) {
    return false;
  }
  if (runtime.server_stream_pending != nullptr) {
    return true;
  }
  if (applied_pending && !runtime.server_stream_batch.active) {
    return true;
  }

  if (!same_batch_target(runtime.server_stream_batch, stream,
                         previous_chunk_view, target_view)) {
    start_server_stream_mesh_batch(runtime.server_stream_batch, stream,
                                   previous_chunk_view, target_view);
  }

  return start_pending_server_stream_build(runtime, stream, block_lookup,
                                           previous_chunk_view, dirty_columns,
                                           visible_frame, frame_index, source,
                                           result);
}

bool run_empty_world_mesh_update(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame,
    world_mesh_gpu_buffers &mesh_buffers, const chunk_view &current_view,
    const chunk_view &previous_chunk_view, const block_lookup &block_lookup,
    uint64_t frame_index, const char *source, int &result) {
  scheduled_world_mesh_update update{gpu_device, &visible_frame, &mesh_buffers,
                                     {},         frame_index,    source};
  empty_world_build_context build{&update, &current_view, &previous_chunk_view,
                                  &block_lookup};
  return execute_scheduled_update(runtime, update, build_empty_world_mesh,
                                  &build, result);
}

bool run_frame_world_mesh_update(
    world_mesh_runtime &runtime, SDL_GPUDevice *gpu_device,
    world_mesh_upload_frame &visible_frame,
    world_mesh_gpu_buffers &mesh_buffers, bool game_modules_disabled,
    bool server_session_enabled, bool has_server_stream,
    bool server_stream_mesh_dirty, bool empty_world_local_edit,
    const server_chunk_stream_file &server_stream,
    const std::vector<empty_world_dirty_column> &server_stream_dirty_columns,
    const chunk_view &current_chunk_view, chunk_view &mesh_chunk_view,
    const block_lookup &block_lookup, uint64_t frame_index, int &result) {
  const bool server_stream_loaded = !server_stream.columns.empty();
  const chunk_view server_stream_view =
      server_stream_loaded ? chunk_view_from_server_stream(server_stream)
                           : current_chunk_view;

  if (game_modules_disabled) {
    if (has_server_stream &&
        (server_stream_mesh_dirty || empty_world_local_edit ||
         runtime.server_stream_batch.active ||
         (server_stream_loaded &&
          !same_chunk_view(mesh_chunk_view, server_stream_view)))) {
      if (empty_world_local_edit) {
        runtime.server_stream_batch.active = false;
        abandon_pending_server_stream_build(runtime);
      }
      const bool applied =
          server_stream_loaded
              ? run_server_stream_world_mesh_update(
                    runtime, gpu_device, visible_frame, mesh_buffers,
                    server_stream, block_lookup, mesh_chunk_view,
                    server_stream_dirty_columns, frame_index,
                    "native_empty_server", result)
              : run_empty_world_mesh_update(
                    runtime, gpu_device, visible_frame, mesh_buffers,
                    current_chunk_view, mesh_chunk_view, block_lookup,
                    frame_index, "native_empty_client", result);
      if (!applied) {
        return false;
      }
      if (!runtime.server_stream_batch.active) {
        mesh_chunk_view = server_stream_view;
      }
    } else if (!server_session_enabled &&
               (!same_chunk_view(mesh_chunk_view, current_chunk_view) ||
                empty_world_local_edit)) {
      visible_frame = {};
      release_world_mesh_gpu_buffers(gpu_device, mesh_buffers);
      if (!run_empty_world_mesh_update(
              runtime, gpu_device, visible_frame, mesh_buffers,
              current_chunk_view, mesh_chunk_view, block_lookup, frame_index,
              "native_empty_client", result)) {
        return false;
      }
      mesh_chunk_view = current_chunk_view;
    }
    return true;
  }

  if (has_server_stream && server_stream_loaded &&
      (server_stream_mesh_dirty || empty_world_local_edit ||
       runtime.server_stream_batch.active ||
       !same_chunk_view(mesh_chunk_view, server_stream_view))) {
    if (empty_world_local_edit) {
      runtime.server_stream_batch.active = false;
      abandon_pending_server_stream_build(runtime);
    }
    if (!run_server_stream_world_mesh_update(
            runtime, gpu_device, visible_frame, mesh_buffers, server_stream,
            block_lookup, mesh_chunk_view, server_stream_dirty_columns,
            frame_index, "server_seed_memory", result)) {
      return false;
    }
    if (!runtime.server_stream_batch.active) {
      mesh_chunk_view = server_stream_view;
    }
  }

  return true;
}

} // namespace octaryn_client_app
