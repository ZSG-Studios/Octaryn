#pragma once
#include "GeometryFormat.h"
#include "PageResidency.h"
#include <string>

namespace octaryn::client::rendering::virtual_geometry {
// Matches Selection.slang; adjacency is precomputed once when loading metadata.
struct SelectionGroup {
  float center[3]{},radius{};
  float error{};
  std::uint32_t first_page{},page_count{},first_parent{},parent_count{},depth{},root{},reserved{};
};
struct SelectionCluster {
  std::uint32_t group{},refined_group{},page{},reserved{};
  float center[3]{},radius{};
};
static_assert(sizeof(SelectionGroup)==48 && sizeof(SelectionCluster)==32);
struct SelectionTopology {
  std::vector<SelectionGroup> groups;
  std::vector<SelectionCluster> clusters;
  std::vector<std::uint32_t> pages,parents;
  std::uint32_t maximum_depth{};
};
struct SelectionView {
  float eye[3]{},focal_pixels{1},error_pixels{1};
  // Inward world-space planes; normals need not be normalized.
  float planes[6][4]{};
  bool frustum{};
};
struct SelectionResult {
  std::vector<std::uint32_t> clusters;
  std::vector<PageRequest> requests;
  std::uint32_t feedback_overflow{},missing_roots{};
};
bool build_selection_topology(const GeometryAsset&,SelectionTopology&,std::string& error);
// Reference for GPU selection. Group refinement is atomic across every parent.
bool select_geometry(const SelectionTopology&,std::span<const GpuPage>,const SelectionView&,
                     std::uint32_t cluster_capacity,std::uint32_t feedback_capacity,
                     SelectionResult&,std::string& error);
}
