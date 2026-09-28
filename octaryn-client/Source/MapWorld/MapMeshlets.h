#pragma once
#include "MapModel.h"
namespace octaryn::client::rendering {
inline constexpr unsigned map_meshlet_vertices=128,map_meshlet_triangles=256;
struct MapMeshlet {
  std::uint32_t vertex_offset{},triangle_offset{},vertex_count{},triangle_count{};
  std::uint32_t material{},padding[3]{};
  float sphere[4]{};
  float cone_apex[3]{},cone_cutoff{};
  float cone_axis[3]{},cone_padding{};
};
static_assert(sizeof(MapMeshlet)==80);
struct MapMeshletData {
  std::vector<MapMeshlet> records;
  std::vector<std::uint32_t> vertices,triangles;
  std::uint64_t bytes() const {return records.size()*sizeof(MapMeshlet)+(vertices.size()+triangles.size())*4;}
};
bool map_meshlet_requested();
bool prepare_map_meshlets(const MapModel&,MapMeshletData&,std::string& error);
}
