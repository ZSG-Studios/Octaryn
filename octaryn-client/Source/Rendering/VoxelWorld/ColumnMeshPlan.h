#pragma once

#include "ChunkView.h"

#include <cstddef>
#include <cstdint>
#include <vector>

enum class column_mesh_plan_action : uint8_t {
  preserve,
  build,
  clear,
};

struct column_mesh_plan_options {
  int32_t center_chunk_x;
  int32_t center_chunk_z;
  int32_t urgent_radius_chunks;
  size_t urgent_submission_budget;
  size_t regular_submission_budget;
};

struct column_mesh_plan_entry {
  int32_t chunk_x;
  int32_t chunk_z;
  int32_t local_x;
  int32_t local_z;
  uint64_t distance_squared;
  column_mesh_plan_action action;
  bool urgent;
};

struct column_mesh_plan_summary {
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

struct column_mesh_plan {
  chunk_view previous_view;
  chunk_view current_view;
  column_mesh_plan_summary summary;
  std::vector<column_mesh_plan_entry> entries;
};

column_mesh_plan_options
column_mesh_plan_default_options(const chunk_view &current_view);

column_mesh_plan build_column_mesh_plan(const chunk_view &previous_view,
                                      const chunk_view &current_view,
                                      const column_mesh_plan_options &options);
