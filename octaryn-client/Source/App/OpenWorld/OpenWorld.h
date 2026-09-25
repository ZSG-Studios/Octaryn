#pragma once
#include "CameraShoulder.h"
#include <string>

namespace octaryn::client::app {

struct WorldRunOptions {
  int frame_limit{};
  std::string capture_ui; // UI canvas capture name; empty disables.
  double benchmark_seconds{};
  bool benchmark_settings{};
  bool benchmark_hidden{};
  bool third_person{};
  CameraShoulder shoulder=CameraShoulder::Right;
  bool validate_frame_pacing{};
  bool show_diagnostics{};
};

int run_open_world(const WorldRunOptions& options);

}
