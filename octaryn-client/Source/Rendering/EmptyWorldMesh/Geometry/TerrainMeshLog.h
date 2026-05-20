#pragma once

#include "ChunkMeshPlan.h"
#include "WorldMeshUpload.h"

#include <cstddef>
#include <cstdint>

namespace octaryn_client_app {
struct server_chunk_stream_file;
}

void log_terrain_stream_mesh_frame(
    const octaryn_client_app::server_chunk_stream_file &stream,
    const chunk_mesh_plan &mesh_plan, size_t selected_column_count,
    size_t override_count, size_t dirty_column_count,
    const world_mesh_upload_frame &mesh_frame);
