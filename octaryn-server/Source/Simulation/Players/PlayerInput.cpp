#include "PlayerSimulation.h"

extern "C" {

uint32_t
octaryn_server_player_has_input_intent(const OctarynServerPlayerInput *input) {
  if (!input) {
    return 0u;
  }

  return input->controller != 0u || input->flags != 0u ||
                 input->move_x != 0.0f || input->move_y != 0.0f ||
                 input->move_z != 0.0f || input->relative_mouse != 0
             ? 1u
             : 0u;
}
}
