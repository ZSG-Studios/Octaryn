#pragma once
#include "TileSet.h"
#include "octaryn_residency_api.h"
#include <algorithm>
#include <cstring>
namespace octaryn::client::rendering {
inline octaryn_host_region_status tile_region_status(const app::WorldTile& tile,std::uint32_t index,std::uint32_t phase,
 bool wanted,bool keep,bool render_ready,bool collision_ready,bool failed,std::uint64_t generation) {
 octaryn_host_region_status out{};out.version=1;out.size=OCTARYN_HOST_REGION_STATUS_SIZE;
 out.index=index;out.phase=phase;out.generation=generation;
 out.flags=(wanted?OCTARYN_REGION_WANTED:0) | (keep?OCTARYN_REGION_RETAINED:0) |
   (render_ready?OCTARYN_REGION_RENDER_READY:0) | (collision_ready?OCTARYN_REGION_COLLISION_READY:0) |
   (failed?OCTARYN_REGION_FAILED:0);
 std::copy_n(tile.bounds,6,out.bounds);std::memcpy(out.id,tile.id.data(),std::min<std::size_t>(128,tile.id.size()));return out;
}
}
