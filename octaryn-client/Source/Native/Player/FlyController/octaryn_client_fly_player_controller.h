#pragma once

#include "octaryn_client_camera.h"
#include "octaryn_client_player_control_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct octaryn_client_fly_player_controller {
  octaryn_client_camera camera;
  float normal_speed_blocks_per_second;
  float sprint_speed_blocks_per_second;
  float mouse_sensitivity_degrees_per_pixel;
} octaryn_client_fly_player_controller;

void octaryn_client_fly_player_controller_init(
    octaryn_client_fly_player_controller *controller);
void octaryn_client_fly_player_controller_reset_spawn(
    octaryn_client_fly_player_controller *controller);
void octaryn_client_fly_player_controller_resize_viewport(
    octaryn_client_fly_player_controller *controller, int width, int height);
void octaryn_client_fly_player_controller_set_position(
    octaryn_client_fly_player_controller *controller, float x, float y,
    float z);
void octaryn_client_fly_player_controller_update(
    octaryn_client_fly_player_controller *controller,
    const octaryn_client_player_control_input *input, float delta_seconds);

#ifdef __cplusplus
}
#endif
