#include "ChunkMeshPlan.h"

#include <algorithm>
#include <cstdlib>

namespace {

constexpr int32_t kDefaultUrgentRadiusChunks = 20;
constexpr size_t kDefaultUrgentSubmissionBudget = 2u;
constexpr size_t kDefaultRegularSubmissionBudget = 1u;

bool valid_view(const chunk_view &view) { return view.width > 0; }

int32_t view_max_x(const chunk_view &view) {
  return view.origin_x + view.width;
}

int32_t view_max_z(const chunk_view &view) {
  return view.origin_z + view.width;
}

bool chunk_inside_view(int32_t chunk_x, int32_t chunk_z,
                       const chunk_view &view) {
  return valid_view(view) && chunk_x >= view.origin_x &&
         chunk_x < view_max_x(view) && chunk_z >= view.origin_z &&
         chunk_z < view_max_z(view);
}

uint64_t distance_squared(int32_t chunk_x, int32_t chunk_z,
                          const chunk_mesh_plan_options &options) {
  const int64_t dx = static_cast<int64_t>(chunk_x) - options.center_chunk_x;
  const int64_t dz = static_cast<int64_t>(chunk_z) - options.center_chunk_z;
  return static_cast<uint64_t>(dx * dx + dz * dz);
}

bool chunk_is_urgent(int32_t chunk_x, int32_t chunk_z,
                     const chunk_mesh_plan_options &options) {
  return std::abs(chunk_x - options.center_chunk_x) <=
             options.urgent_radius_chunks &&
         std::abs(chunk_z - options.center_chunk_z) <=
             options.urgent_radius_chunks;
}

chunk_mesh_plan_entry make_entry(int32_t chunk_x, int32_t chunk_z,
                                 const chunk_view &view,
                                 chunk_mesh_plan_action action,
                                 const chunk_mesh_plan_options &options) {
  return {
      chunk_x,
      chunk_z,
      chunk_x - view.origin_x,
      chunk_z - view.origin_z,
      distance_squared(chunk_x, chunk_z, options),
      action,
      action == chunk_mesh_plan_action::build &&
          chunk_is_urgent(chunk_x, chunk_z, options),
  };
}

int entry_sort_key(const chunk_mesh_plan_entry &entry) {
  if (entry.action == chunk_mesh_plan_action::build && entry.urgent) {
    return 0;
  }
  if (entry.action == chunk_mesh_plan_action::build) {
    return 1;
  }
  if (entry.action == chunk_mesh_plan_action::preserve) {
    return 2;
  }
  return 3;
}

void add_current_entries(chunk_mesh_plan &plan,
                         const chunk_mesh_plan_options &options) {
  if (!valid_view(plan.current_view)) {
    return;
  }

  for (int32_t chunk_z = plan.current_view.origin_z;
       chunk_z < view_max_z(plan.current_view); ++chunk_z) {
    for (int32_t chunk_x = plan.current_view.origin_x;
         chunk_x < view_max_x(plan.current_view); ++chunk_x) {
      const bool preserved =
          chunk_inside_view(chunk_x, chunk_z, plan.previous_view);
      const auto action = preserved ? chunk_mesh_plan_action::preserve
                                    : chunk_mesh_plan_action::build;
      chunk_mesh_plan_entry entry =
          make_entry(chunk_x, chunk_z, plan.current_view, action, options);
      if (preserved) {
        ++plan.summary.preserved_columns;
      } else {
        ++plan.summary.loaded_columns;
        if (entry.urgent) {
          ++plan.summary.urgent_jobs;
        } else {
          ++plan.summary.regular_jobs;
        }
      }
      plan.entries.push_back(entry);
    }
  }
}

void add_clear_entries(chunk_mesh_plan &plan,
                       const chunk_mesh_plan_options &options) {
  if (!valid_view(plan.previous_view)) {
    return;
  }

  for (int32_t chunk_z = plan.previous_view.origin_z;
       chunk_z < view_max_z(plan.previous_view); ++chunk_z) {
    for (int32_t chunk_x = plan.previous_view.origin_x;
         chunk_x < view_max_x(plan.previous_view); ++chunk_x) {
      if (chunk_inside_view(chunk_x, chunk_z, plan.current_view)) {
        continue;
      }
      plan.entries.push_back(make_entry(chunk_x, chunk_z, plan.previous_view,
                                        chunk_mesh_plan_action::clear,
                                        options));
      ++plan.summary.unloaded_columns;
      ++plan.summary.clear_jobs;
    }
  }
}

void sort_plan_entries(std::vector<chunk_mesh_plan_entry> &entries) {
  std::sort(entries.begin(), entries.end(),
            [](const chunk_mesh_plan_entry &left,
               const chunk_mesh_plan_entry &right) {
              const int left_action = entry_sort_key(left);
              const int right_action = entry_sort_key(right);
              if (left_action != right_action) {
                return left_action < right_action;
              }
              if (left.action == chunk_mesh_plan_action::clear &&
                  right.action == chunk_mesh_plan_action::clear &&
                  left.distance_squared != right.distance_squared) {
                return left.distance_squared > right.distance_squared;
              }
              if (left.distance_squared != right.distance_squared) {
                return left.distance_squared < right.distance_squared;
              }
              if (left.local_x != right.local_x) {
                return left.local_x < right.local_x;
              }
              return left.local_z < right.local_z;
            });
}

} // namespace

chunk_mesh_plan_options
chunk_mesh_plan_default_options(const chunk_view &current_view) {
  return {
      current_view.origin_x + current_view.width / 2,
      current_view.origin_z + current_view.width / 2,
      kDefaultUrgentRadiusChunks,
      kDefaultUrgentSubmissionBudget,
      kDefaultRegularSubmissionBudget,
  };
}

chunk_mesh_plan build_chunk_mesh_plan(const chunk_view &previous_view,
                                      const chunk_view &current_view,
                                      const chunk_mesh_plan_options &options) {
  chunk_mesh_plan plan{};
  plan.previous_view = previous_view;
  plan.current_view = current_view;
  plan.summary.active_columns =
      valid_view(current_view) ? static_cast<size_t>(current_view.width) *
                                     static_cast<size_t>(current_view.width)
                               : 0u;

  if (valid_view(current_view)) {
    plan.entries.reserve(plan.summary.active_columns);
  }
  add_current_entries(plan, options);
  add_clear_entries(plan, options);
  sort_plan_entries(plan.entries);

  plan.summary.scheduled_urgent_jobs =
      std::min(plan.summary.urgent_jobs, options.urgent_submission_budget);
  plan.summary.scheduled_regular_jobs =
      std::min(plan.summary.regular_jobs, options.regular_submission_budget);
  return plan;
}
