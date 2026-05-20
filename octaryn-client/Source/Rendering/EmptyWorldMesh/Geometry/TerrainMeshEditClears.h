#pragma once

#include "EmptyWorldMesh.h"

void append_guarded_override_section_clears(
    world_mesh_upload_frame &mesh_frame,
    const octaryn_client_app::block_lookup &overrides,
    const std::vector<chunk_mesh_plan_entry> &selected,
    const world_mesh_upload_frame &replacement_frame);
