#include "WorldMeshRuntime.h"

#include "FrameProfile.h"
#include "Log.h"
#include "octaryn_native_schedule_runtime.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
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
  size_t first_plan_entry = 0u;
  size_t max_plan_entries = 0u;
  empty_world_stream_mesh_batch_result *batch_result = nullptr;
};

struct empty_world_build_context {
  scheduled_world_mesh_update *update = nullptr;
  const chunk_view *current_view = nullptr;
  const chunk_view *previous_chunk_view = nullptr;
  const block_lookup *block_lookup = nullptr;
};

int build_server_stream_mesh(void *context) {
  auto *build = static_cast<server_stream_build_context *>(context);
  const uint64_t start_ticks = SDL_GetTicksNS();
  build_empty_world_mesh_frame_from_stream_batch(
      *build->stream, *build->block_lookup, *build->previous_chunk_view,
      *build->dirty_columns, build->first_plan_entry, build->max_plan_entries,
      build->update->update_frame, *build->batch_result);
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

size_t server_stream_mesh_batch_budget() {
  constexpr size_t kDefaultBudget = 12u;
  constexpr size_t kMaxBudget = 128u;
  const char *value =
      std::getenv("OCTARYN_CLIENT_SERVER_STREAM_MESH_COLUMNS_PER_FRAME");
  if (value == nullptr || value[0] == '\0') {
    return kDefaultBudget;
  }

  const long parsed = std::strtol(value, nullptr, 10);
  return std::clamp(parsed <= 0 ? kDefaultBudget : static_cast<size_t>(parsed),
                    size_t{1u}, kMaxBudget);
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
  if (!same_batch_target(runtime.server_stream_batch, stream,
                         previous_chunk_view, target_view)) {
    start_server_stream_mesh_batch(runtime.server_stream_batch, stream,
                                   previous_chunk_view, target_view);
  }

  scheduled_world_mesh_update update{gpu_device, &visible_frame, &mesh_buffers,
                                     {},         frame_index,    source};
  empty_world_stream_mesh_batch_result batch_result{};
  server_stream_build_context build{&update,
                                    &stream,
                                    &block_lookup,
                                    &previous_chunk_view,
                                    &dirty_columns,
                                    runtime.server_stream_batch.next_plan_entry,
                                    server_stream_mesh_batch_budget(),
                                    &batch_result};
  if (!execute_scheduled_update(runtime, update, build_server_stream_mesh,
                                &build, result)) {
    return false;
  }

  runtime.server_stream_batch.next_plan_entry = batch_result.next_entry;
  ++runtime.server_stream_batch.batch_index;
  runtime.server_stream_batch.active = !batch_result.complete;
  if (g_log != nullptr) {
    std::fprintf(
        g_log,
        "live_server_stream_mesh_batch frame=%" PRIu64
        " active=1 source=%s epoch=%" PRIu64
        " batch=%zu first_entry=%zu next_entry=%zu processed=%zu"
        " remaining=%zu complete=%d build_columns=%zu clear_columns=%zu"
        " radius=%" PRIu32 " columns=%zu chunks=%zu opaque_faces=%zu"
        " build_ms=%.3f upload_ms=%.3f\n",
        frame_index, source, stream.epoch,
        runtime.server_stream_batch.batch_index, batch_result.first_entry,
        batch_result.next_entry, batch_result.processed_entries,
        batch_result.remaining_entries, batch_result.complete ? 1 : 0,
        batch_result.build_columns, batch_result.clear_columns, stream.radius,
        stream.columns.size(), update.update_frame.chunks.size(),
        update.update_frame.opaque_faces.size(), update.build_ms,
        update.upload_ms);
    std::fflush(g_log);
  }
  return true;
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
