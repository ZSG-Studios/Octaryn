#pragma once

#include "PlayerControlInput.h"
#include "RuntimeControls.h"
#include "CameraShoulder.h"
#include <SDL3/SDL.h>

namespace octaryn::client::app {

class DebugOverlay;

struct WorldControls {
  DebugOverlay* overlay{};
  bool third_person=false;
  CameraShoulder shoulder=CameraShoulder::Right;
  player_control_input movement{};
  float yaw = 0.0f;
  float pitch = -0.15f;
  bool running = true;
  bool captured = false;
  bool flying = true;
  bool resized = false;
  bool display_changed = false;
  unsigned zoom = 0;
  int time_hour_steps = 0;
  runtime_controls ui{};
};

// Pumps the SDL event queue for one frame: window lifecycle, engine hotkeys,
// look input and fly movement keys. Clicking the window captures the mouse;
// Escape releases it.
void read_world_controls(SDL_Window* window, WorldControls& controls, bool interactive = true);

}
