#include "octaryn_client_player_control_input.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace {

int key_down(const bool *keyboard_state, int keyboard_state_count,
             SDL_Scancode scancode) {
  const int index = static_cast<int>(scancode);
  return keyboard_state != nullptr && index >= 0 &&
         index < keyboard_state_count && keyboard_state[index];
}

} // namespace

void octaryn_client_player_control_input_clear(
    octaryn_client_player_control_input *input) {
  if (input == nullptr) {
    return;
  }

  std::memset(input, 0, sizeof(*input));
}

void octaryn_client_player_control_input_read_sdl_keyboard(
    octaryn_client_player_control_input *input, const bool *keyboard_state,
    int keyboard_state_count) {
  if (input == nullptr) {
    return;
  }

  input->move_forward =
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_W);
  input->move_backward =
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_S);
  input->move_left =
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_A);
  input->move_right =
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_D);
  input->move_up =
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_E) ||
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_SPACE);
  input->move_down =
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_Q) ||
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_LSHIFT);
  input->sprint =
      key_down(keyboard_state, keyboard_state_count, SDL_SCANCODE_LCTRL);
}

void octaryn_client_player_control_input_set_mouse_delta(
    octaryn_client_player_control_input *input, float yaw_delta,
    float pitch_delta) {
  if (input == nullptr) {
    return;
  }

  input->mouse_yaw_delta = yaw_delta;
  input->mouse_pitch_delta = pitch_delta;
}
