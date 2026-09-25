#include "WorldRendererInternal.h"
#include <slang-rhi/shader-cursor.h>

namespace octaryn::client::rendering {
bool world_ray_lighting_initialize(WorldRenderer& r) {
  // The voxel ray-water pipeline is archived with the voxel world; ray-lit
  // raster pipelines for streamed world geometry return here.
  (void)r;
  return true;
}
}
