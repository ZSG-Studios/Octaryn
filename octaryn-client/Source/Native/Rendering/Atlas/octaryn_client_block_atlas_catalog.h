#pragma once

#include "octaryn_client_block_atlas.h"

namespace octaryn::client::rendering {

bool load_client_block_atlas_catalog_metadata(FILE *log,
                                              ClientBlockAtlas &atlas);

} // namespace octaryn::client::rendering
