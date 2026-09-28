#pragma once
#include "MapLodCache.h"
namespace octaryn::client::rendering {
struct MapRenderer;
struct MapIndirectPrimitive {
  float minimum[4]{},maximum[4]{};
  std::uint32_t first{},count{},material{},opaque{};
  std::uint32_t lods[4]{};float errors[4]{};
};
static_assert(sizeof(MapIndirectPrimitive)==80);
bool prepare_map_indirect_data(MapRenderer&,const MapLodData&,float pixels,std::vector<MapIndirectPrimitive>&);
}
