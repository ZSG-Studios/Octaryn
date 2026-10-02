#pragma once
#include "MapModel.h"
namespace octaryn::client::rendering {
inline MapLoadLimits tile_load_limits() {
 MapLoadLimits limits;limits.source_bytes=limits.encoded_bytes=64ull*1024*1024;
 limits.triangles=131072;limits.primitives=2048;limits.accessor_elements=393216;
 limits.geometry_bytes=64ull*1024*1024;return limits;
}
}
