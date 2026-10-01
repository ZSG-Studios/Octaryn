#pragma once
#include "MapModel.h"
#include <fastgltf/types.hpp>
#include <array>

namespace octaryn::client::rendering {
struct MapSourcePrimitive {
  std::size_t mesh{},primitive{},vertices{},triangles{},decoded_vertices{};
};
struct MapSourceInstance {
  std::size_t node{},mesh{};
  std::string name;
  std::array<float,16> transform{};
};
struct MapSourceInfo {
  std::uint64_t external_bytes{},logical_bytes{},unique_triangles{},instanced_triangles{};
  std::size_t mesh_count{},material_count{},compressed_views{};
  std::vector<MapSourcePrimitive> primitives;
  std::vector<MapSourceInstance> instances;
};
// Parses only scene metadata. Instance transforms remain separate from shared meshes.
bool inspect_map_source(const std::filesystem::path&,MapSourceInfo&,std::string& error);
// Object-space primitive preparation for a bounded offline cook; never flattens other instances.
bool load_map_source_primitive(const std::filesystem::path&,std::size_t mesh,std::size_t primitive,
    MapModel&,std::string& error,const MapLoadLimits& limits={});
void qualify_map_primitive(const fastgltf::Asset&,const fastgltf::Primitive&,const MapLoadLimits&);
void qualify_map_scene(const fastgltf::Asset&,const MapLoadLimits&,bool catalog);
}
