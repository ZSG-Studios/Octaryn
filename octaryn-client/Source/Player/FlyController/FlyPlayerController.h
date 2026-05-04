#pragma once

#include "Camera.h"
#include "PlayerControlInput.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fly_player_controller {
  camera camera;
  float normal_speed_blocks_per_second;
  float sprint_speed_blocks_per_second;
  float mouse_sensitivity_degrees_per_pixel;
} fly_player_controller;

void fly_player_controller_init(
    fly_player_controller *controller);
void fly_player_controller_reset_spawn(
    fly_player_controller *controller);
void fly_player_controller_resize_viewport(
    fly_player_controller *controller, int width, int height);
void fly_player_controller_set_position(
    fly_player_controller *controller, float x, float y,
    float z);
void fly_player_controller_update(
    fly_player_controller *controller,
    const player_control_input *input, float delta_seconds);

#ifdef __cplusplus
}
#endif
