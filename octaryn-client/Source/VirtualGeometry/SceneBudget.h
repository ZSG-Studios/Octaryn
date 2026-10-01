#pragma once
#include "SceneCatalog.h"
#include "GeometryBudget.h"

namespace octaryn::client::rendering::virtual_geometry {
inline std::uint64_t scene_part_reservation(const ScenePart& part,const ScenePrimitive& primitive,std::uint64_t nodes) {
  if(part.geometry.empty())return 0;
  const auto forward=primitive.surface.alpha_mode==MapAlphaMode::Blend?part.triangle_count*3*(sizeof(MapVertex)+8):0;
  return geometry_raster_reservation(part.pages,part.clusters)+geometry_ray_reservation(part.clusters)+forward+
      geometry_ray_instance_reservation(part.clusters,nodes)+geometry_instance_view_reservation(nodes);
}
// Every successfully cooked nonempty part has at least one page and cluster.
// Include fixed owner reservations before preparing a neighborhood that cannot fit.
inline std::uint64_t scene_part_minimum_reservation(const ScenePart& part,const ScenePrimitive& primitive,std::uint64_t nodes) {
  if(!part.geometry.empty())return scene_part_reservation(part,primitive,nodes);
  const auto forward=primitive.surface.alpha_mode==MapAlphaMode::Blend?part.triangle_count*3*(sizeof(MapVertex)+8):0;
  return geometry_raster_reservation(1,1)+geometry_ray_reservation(1)+forward+geometry_ray_instance_reservation(1,nodes)+
      geometry_instance_view_reservation(nodes);
}
}
