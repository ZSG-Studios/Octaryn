#pragma once

#include "PlayerControlInput.h"
#include "RuntimeControls.h"
#include "ActionQueue.h"
#include "CameraShoulder.h"
#include <SDL3/SDL.h>

namespace octaryn::client::app {
class LightingPanel;
class GameUi;
struct WorldControls {
  LightingPanel* lighting{};
  GameUi* game_ui{};
  bool third_person=false;
  CameraShoulder shoulder=CameraShoulder::Right;
  player_control_input movement{};
  float yaw = 0.0f;
  float pitch = -0.15f;
  bool running = true;
  bool captured = false;
  bool flying = false;
  bool resized = false;
  unsigned zoom = 0;
  int time_hour_steps = 0;
  BlockActionQueue actions;
  bool action_overflow_reported=false;
  runtime_controls ui{};
};
void read_world_controls(SDL_Window* window, WorldControls& controls, bool interactive = true);
}
