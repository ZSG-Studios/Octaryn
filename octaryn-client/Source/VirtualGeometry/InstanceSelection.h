#pragma once
#include "GeometryTransform.h"
#include <cstddef>

namespace octaryn::client::rendering::virtual_geometry {
// Scalar/std430-safe GPU layout: every row is an explicit 16-byte vector.
struct InstanceSelectionView {
  float eye_focal[4]{};
  float error_scale[4]{1,1,0,0};
  float world[3][4]{{1,0,0,0},{0,1,0,0},{0,0,1,0}};
  float planes[6][4]{};
  std::uint32_t flags[4]{};
};
static_assert(sizeof(InstanceSelectionView)==192);
static_assert(offsetof(InstanceSelectionView,world)==32 && offsetof(InstanceSelectionView,planes)==80 &&
    offsetof(InstanceSelectionView,flags)==176);
InstanceSelectionView instance_selection_view(const SelectionView&,const GeometryTransform&);
float instance_selection_error(const InstanceSelectionView&,const GeometryBounds&);
bool valid_instance_selection(std::span<const InstanceSelectionView>);
// Selects one complete DAG cut refined for every instance; never concatenates overlapping cuts.
bool select_geometry_instances(const SelectionTopology&,std::span<const GpuPage>,std::span<const InstanceSelectionView>,
    std::uint32_t cluster_capacity,std::uint32_t feedback_capacity,SelectionResult&,std::string& error);
}
