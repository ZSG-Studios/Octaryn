#pragma once

#include "Camera.h"
#include "Input.h"
#include "RuntimeControls.h"

#include <array>
#include <cstdint>

namespace octaryn_client_app {

constexpr uint32_t kMaxPlayerModelVertices = 256u;

struct player_model_render_vertex {
  float position[4]{};
  float color[4]{};
};

struct player_model_frame_vertices {
  std::array<player_model_render_vertex, kMaxPlayerModelVertices> vertices{};
  uint32_t count = 0u;
  const char *animation_name = "idle_loop";
  bool asset_loaded = false;
};

bool build_player_model_frame(const camera &player_camera,
                              const client_input_debug_state &input,
                              const runtime_controls &controls,
                              uint64_t frame_index,
                              player_model_frame_vertices &frame);

} // namespace octaryn_client_app
