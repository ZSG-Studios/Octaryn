#pragma once
#include "MapModel.h"

namespace octaryn::client::rendering {
struct MapForwardGeometry {
  std::vector<MapVertex> vertices;
  std::vector<std::uint32_t> indices,first_indices;
};
// Keeps original model/material indices intact for collision and VG cooking.
bool build_map_forward_geometry(const MapModel&,MapForwardGeometry&,std::string& error,
    std::uint64_t byte_budget=512ull*1024*1024);
}
