#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct player_control_input {
  int move_forward;
  int move_backward;
  int move_left;
  int move_right;
  int move_up;
  int move_down;
  int sprint;
  float mouse_yaw_delta;
  float mouse_pitch_delta;
} player_control_input;

void player_control_input_clear(
    player_control_input *input);
void player_control_input_read_sdl_keyboard(
    player_control_input *input, const bool *keyboard_state,
    int keyboard_state_count);
void player_control_input_set_mouse_delta(
    player_control_input *input, float yaw_delta,
    float pitch_delta);

#ifdef __cplusplus
}
#endif
