#pragma once
#include "MapModel.h"
#include <array>
namespace octaryn::client::rendering {
inline constexpr unsigned map_lod_cache_version=2;
struct MapLodLevel {std::uint32_t first{},count{};float error{};};
static_assert(sizeof(MapLodLevel)==12);
struct MapLodData {
  std::vector<std::array<MapLodLevel,2>> primitives;
  std::vector<std::uint32_t> indices;
};
bool read_map_lods(const std::filesystem::path&,const std::string& source_hash,const MapModel&,MapLodData&,std::string& error);
bool write_map_lods(const std::filesystem::path&,const std::string& source_hash,const MapModel&,const MapLodData&,std::string& error);
}
