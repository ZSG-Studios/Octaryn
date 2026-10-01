#pragma once
#include "AnimationAsset.h"
#include "../VirtualGeometry/GeometryFormat.h"

namespace octaryn::client::animation {
struct ClusterSource {std::uint32_t primitive{},first_vertex{};};
struct CookedAsset {
  Asset animation;
  // One material per primitive, matching GeometryCluster::material.
  std::vector<rendering::MapMaterial> materials;
  std::vector<rendering::MapModelImage> images;
  rendering::virtual_geometry::GeometryAsset geometry;
  std::vector<ClusterSource> cluster_sources;
  // Primitive-local indices into SourceVertex and each target-major morph array.
  std::vector<std::uint32_t> source_vertices;
};
bool validate_cooked_animation(const CookedAsset&,std::string& error);
}
