#pragma once

#include "EmptyWorldMesh.h"
#include "WorldMeshUpload.h"

#include <vector>

namespace octaryn_client_app {

std::vector<empty_world_retained_column> retained_columns_from_frame(
    const world_mesh_upload_frame &frame);

} // namespace octaryn_client_app
