#pragma once
#include "MapModel.h"
#include <array>
#include <map>
#include <set>
#include <sstream>
#include <span>

namespace octaryn::tools::tiles {
using namespace client::rendering;
struct Triangle {std::uint32_t primitive{},first{};};
struct ImageFile {std::string uri;};
struct TextureFiles {
  std::vector<ImageFile> images;
  std::vector<std::set<std::string>> materials;
  std::map<std::string,std::uint64_t> bytes;
};
struct TileResult {std::string file;std::array<float,6> bounds;std::uint32_t triangles{};};
enum class PartitionOrder { Input,Morton };
struct Settings {unsigned triangles{16384},max_tiles{4096};float cell{32};std::uint64_t texture_budget{96u*1024*1024};PartitionOrder order{PartitionOrder::Input};};
void order_triangles(const MapModel&,std::vector<Triangle>&,const std::array<int,3>& cell,const Settings&);
bool compare_tile_cooks(const std::filesystem::path&,const std::filesystem::path&,
    const std::filesystem::path& receipt={});
bool prepare_textures(const MapModel&,const std::filesystem::path& cache,const std::filesystem::path& output,
    TextureFiles&,std::string& error);
bool write_tile(const MapModel&,std::span<const Triangle>,const TextureFiles&,const std::filesystem::path&,
    TileResult&,std::string& error);
bool partition(const MapModel&,const TextureFiles&,const Settings&,const std::filesystem::path&,
    std::vector<TileResult>&,std::string& error);
std::string material_json(const MapMaterial&,const std::array<int,5>&);
std::ostringstream json_stream();
}
