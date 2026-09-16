#pragma once
#include "CameraShoulder.h"
#include <string>

namespace octaryn::client::app {
struct WorldRunOptions {
  int frame_limit{};
  int render_distance{}; // Zero preserves the saved setting.
  std::string capture_ui; // UI canvas capture name; empty disables.
  double benchmark_seconds{};
  double benchmark_streaming_speed{};
  bool benchmark_settings{};
  bool benchmark_hidden{};
  bool show_settings{};
  bool show_fsr_settings{};
  bool show_inventory{},show_creative{},show_menu{};
  bool third_person{},show_lighting{};
  CameraShoulder shoulder=CameraShoulder::Right;
  bool validate_ui{};
  bool validate_distance_changes{};
  bool validate_world_items{};
  bool validate_temporal{};
  bool validate_lighting_motion{};
  bool validate_lighting_edits{};
  bool show_diagnostics{};
};
int run_open_world(const WorldRunOptions& options);
}
