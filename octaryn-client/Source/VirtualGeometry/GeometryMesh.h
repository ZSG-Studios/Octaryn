#pragma once
#include "GeometryFormat.h"
#include <array>

namespace octaryn::client::rendering::virtual_geometry {
struct GeometryMesh {
  std::vector<MapVertex> vertices;
  std::vector<unsigned> indices;
  std::vector<std::array<float,23>> attributes;
  std::vector<unsigned char> locks;
};
GeometryMesh geometry_mesh(const MapModel&,const MapPrimitive&,bool position_only,bool filter_redundant_faces=false);
}
