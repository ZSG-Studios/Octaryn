#pragma once

#include "ChunkView.h"

#include <cstddef>
#include <cstdint>
#include <vector>

enum class chunk_mesh_plan_action : uint8_t {
  preserve,
  build,
  clear,
};

struct chunk_mesh_plan_options {
  int32_t center_chunk_x;
  int32_t center_chunk_z;
  int32_t urgent_radius_chunks;
  size_t urgent_submission_budget;
  size_t regular_submission_budget;
};

struct chunk_mesh_plan_entry {
  int32_t chunk_x;
  int32_t chunk_z;
  int32_t local_x;
  int32_t local_z;
  uint64_t distance_squared;
  chunk_mesh_plan_action action;
  bool urgent;
};

struct chunk_mesh_plan_summary {
  size_t active_columns;
  size_t loaded_columns;
  size_t preserved_columns;
  size_t unloaded_columns;
  size_t urgent_jobs;
  size_t regular_jobs;
  size_t clear_jobs;
  size_t scheduled_urgent_jobs;
  size_t scheduled_regular_jobs;
};

struct chunk_mesh_plan {
  chunk_view previous_view;
  chunk_view current_view;
  chunk_mesh_plan_summary summary;
  std::vector<chunk_mesh_plan_entry> entries;
};

chunk_mesh_plan_options
chunk_mesh_plan_default_options(const chunk_view &current_view);

chunk_mesh_plan build_chunk_mesh_plan(const chunk_view &previous_view,
                                      const chunk_view &current_view,
                                      const chunk_mesh_plan_options &options);
