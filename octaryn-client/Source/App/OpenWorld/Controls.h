#pragma once

#include "PlayerControlInput.h"
#include "RuntimeControls.h"
#include "ActionQueue.h"
#include "CameraShoulder.h"
#include "JumpInputEvents.h"
#include <SDL3/SDL.h>

namespace octaryn::client::app {
class LightingPanel;
class GameUi;
struct LocalPlayerInput;
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
  bool breaking = false;
  bool resized = false;
  bool display_changed = false;
  unsigned zoom = 0;
  int time_hour_steps = 0;
  BlockActionQueue actions;
  bool action_overflow_reported=false;
  JumpInputEvents jump_events;
  bool jump_events_enabled{};
  runtime_controls ui{};
};
void read_world_controls(SDL_Window* window, WorldControls& controls, bool interactive = true);
void take_jump_input(WorldControls& controls, LocalPlayerInput& input);
}
