#pragma once
#include "Selection.h"
#include <array>
#include <string>

namespace octaryn::client::rendering::virtual_geometry {
struct GeometryTransform {
  // Row-major affine matrices match float4 shader uniforms and TLAS transforms.
  std::array<float,12> world{1,0,0,0,0,1,0,0,0,0,1,0};
  std::array<float,12> inverse{1,0,0,0,0,1,0,0,0,0,1,0};
  std::array<float,12> normal{1,0,0,0,0,1,0,0,0,0,1,0};
  float scale{1},inverse_scale{1},orientation{1};
};
bool geometry_transform(const std::array<float,16>& column_major,GeometryTransform&,std::string& error);
std::array<float,3> geometry_transform_point(const std::array<float,12>&,const std::array<float,3>&);
std::array<float,6> geometry_transform_bounds(const GeometryTransform&,const std::array<float,6>&);
SelectionView geometry_local_view(const GeometryTransform&,const SelectionView&);
}
