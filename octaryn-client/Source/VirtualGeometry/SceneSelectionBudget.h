#pragma once
#include "Selection.h"
#include "InstanceSelection.h"

namespace octaryn::client::rendering::virtual_geometry {
struct SelectionResourcesConfig {
  std::uint32_t groups{2048},clusters{8192},pages{1024},page_references{8192},parents{16384},instances{4096};
  std::uint32_t feedback_capacity{1024},frame_count{2},tickets_per_frame{8192};
  std::uint64_t readback_bytes{4ull*1024*1024};
};
// Physical capacities of the shared frame banks; shader pipeline storage is driver-owned.
inline std::uint64_t scene_selection_bytes(const SelectionResourcesConfig& c) {
  const std::uint64_t topology=std::uint64_t(c.groups)*sizeof(SelectionGroup)+
      std::uint64_t(c.clusters)*sizeof(SelectionCluster)+(std::uint64_t(c.page_references)+c.parents)*4;
  const std::uint64_t scratch=std::uint64_t(c.pages)*28+std::uint64_t(c.groups)*4+
      std::uint64_t(c.feedback_capacity)*8+std::uint64_t(c.clusters)*16+48+
      std::uint64_t(c.instances)*sizeof(InstanceSelectionView)+c.readback_bytes;
  return (topology+scratch)*c.frame_count;
}
inline std::uint64_t scene_selection_feedback_bytes(std::uint32_t pages,std::uint32_t capacity) {
  return (24+std::uint64_t(capacity)*8+std::uint64_t(pages)*4+15)&~std::uint64_t(15);
}
}
