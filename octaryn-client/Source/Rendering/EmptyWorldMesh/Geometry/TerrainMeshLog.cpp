#include "TerrainMeshLog.h"

#include "JsonContracts.h"
#include "Log.h"

#include <cinttypes>
#include <cstdio>

void log_terrain_stream_mesh_frame(
    const octaryn_client_app::server_chunk_stream_file &stream,
    const chunk_mesh_plan &mesh_plan, size_t selected_column_count,
    size_t override_count, size_t dirty_column_count,
    const world_mesh_upload_frame &mesh_frame) {
  if (octaryn_client_app::g_log == nullptr) {
    return;
  }

  std::fprintf(
      octaryn_client_app::g_log,
      "native_empty_chunk_stream active=1 "
      "source=server_seed_memory epoch=%" PRIu64 " render_distance=%" PRIu32
      " columns=%zu loaded=%zu preserved=%zu override_edits=%zu "
      "dirty_columns=%zu visible_chunks=%zu opaque_faces=%zu "
      "transparent_faces=%zu fluid_blocks=%" PRIu32 " "
      "unloaded=%zu active_columns=%zu plan_entries=%zu "
      "urgent_jobs=%zu regular_jobs=%zu clear_jobs=%zu "
      "scheduled_urgent=%zu scheduled_regular=%zu\n",
      stream.epoch, stream.radius, selected_column_count,
      mesh_plan.summary.loaded_columns, mesh_plan.summary.preserved_columns,
      override_count, dirty_column_count, mesh_frame.chunks.size(),
      mesh_frame.opaque_faces.size(), mesh_frame.transparent_faces.size(),
      mesh_frame.fluid_blocks, mesh_plan.summary.unloaded_columns,
      mesh_plan.summary.active_columns, mesh_plan.entries.size(),
      mesh_plan.summary.urgent_jobs, mesh_plan.summary.regular_jobs,
      mesh_plan.summary.clear_jobs, mesh_plan.summary.scheduled_urgent_jobs,
      mesh_plan.summary.scheduled_regular_jobs);
  std::fflush(octaryn_client_app::g_log);
}
