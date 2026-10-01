#pragma once
#include "MapLodCache.h"
namespace octaryn::client::rendering {
MapLodData cook_map_lods(const MapModel&);
bool test_map_lods(const std::filesystem::path&);
}
