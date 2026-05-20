#pragma once

#include "JsonContracts.h"
#include "PresentationState.h"
#include "WorldMeshUpload.h"

#include <vector>

void audit_terrain_mesh_surface_coverage(
    const std::vector<octaryn_client_app::server_chunk_stream_column_record>
        &columns,
    const octaryn_client_app::block_lookup &overrides,
    const world_mesh_upload_frame &mesh_frame);
