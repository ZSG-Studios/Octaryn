#include "PlayerSimulation.h"

#include <cmath>
#include <cstdio>
#include <string_view>

namespace {

bool expect_true(std::string_view label, bool value) {
  if (value) {
    return true;
  }

  std::fprintf(stderr, "%.*s: expected true\n", static_cast<int>(label.size()),
               label.data());
  return false;
}

bool expect_close(std::string_view label, float actual, float expected) {
  if (std::fabs(actual - expected) <= 0.001f) {
    return true;
  }

  std::fprintf(stderr, "%.*s: value mismatch actual=%f expected=%f\n",
               static_cast<int>(label.size()), label.data(), actual, expected);
  return false;
}

OctarynServerPlayerState default_state() {
  return OctarynServerPlayerState{.x = 0.0f,
                                  .y = 80.0f,
                                  .z = 0.0f,
                                  .pitch = -0.35f,
                                  .yaw = 0.0f,
                                  .velocity_x = 0.0f,
                                  .velocity_y = 0.0f,
                                  .velocity_z = 0.0f,
                                  .is_on_ground = 0u,
                                  .control_mode = 0u,
                                  .selected_block = 25u,
                                  .reserved = 0u};
}

} // namespace

bool validate_save_state_projection() {
  OctarynServerPlayerState state = default_state();
  state.x = 1.0f;
  state.y = 2.0f;
  state.z = 3.0f;
  state.pitch = 0.25f;
  state.yaw = 0.5f;
  state.selected_block = 9u;
  OctarynServerPlayerSaveState save_state{};
  const int result =
      octaryn_server_player_save_state_from_state(&state, &save_state);

  bool ok = true;
  ok &= expect_true("save state projection result", result == 0);
  ok &= expect_close("save state projection x", save_state.x, 1.0f);
  ok &= expect_close("save state projection y", save_state.y, 2.0f);
  ok &= expect_close("save state projection z", save_state.z, 3.0f);
  ok &= expect_close("save state projection pitch", save_state.pitch, 0.25f);
  ok &= expect_close("save state projection yaw", save_state.yaw, 0.5f);
  ok &= expect_true("save state projection block",
                    save_state.selected_block == 9u);
  ok &= expect_true("save state projection rejects null state",
                    octaryn_server_player_save_state_from_state(
                        nullptr, &save_state) != 0);
  ok &= expect_true("save state projection rejects null output",
                    octaryn_server_player_save_state_from_state(&state,
                                                                nullptr) != 0);
  return ok;
}

bool validate_session_save_bookkeeping() {
  OctarynServerPlayerState state = default_state();
  OctarynServerPlayerSession session{};
  bool ok = true;
  ok &= expect_true("session create result",
                    octaryn_server_player_session_from_state(&state, 0u,
                                                             &session) == 0);
  ok &= expect_true("session starts unloaded", session.loaded_from_save == 0u);
  ok &= expect_true("session starts without elapsed save time",
                    session.seconds_since_last_save == 0.0);

  OctarynServerPlayerSessionSaveResult save_result{};
  ok &= expect_true("session unchanged save decision",
                    octaryn_server_player_session_save_decision(
                        &session, 2.0, 1u, &save_result) == 0);
  ok &= expect_true("session unchanged does not save",
                    save_result.should_save == 0u);
  ok &= expect_true("session unchanged tracks elapsed save time",
                    session.seconds_since_last_save == 2.0);

  session.state.x += 0.25f;
  ok &= expect_true("session changed saves after elapsed idle",
                    octaryn_server_player_session_save_decision(
                        &session, 0.25, 0u, &save_result) == 0);
  ok &= expect_true("session changed save flag",
                    save_result.should_save == 1u);
  ok &= expect_close("session save result x", save_result.save_state.x,
                     state.x + 0.25f);
  ok &= expect_true("session resets elapsed after save decision",
                    session.seconds_since_last_save == 0.0);

  ok &= expect_true("session note saved",
                    octaryn_server_player_session_note_saved(
                        &session, &save_result.save_state) == 0);
  ok &= expect_true("session saved state becomes baseline",
                    octaryn_server_player_session_save_decision(
                        &session, 2.0, 1u, &save_result) == 0);
  ok &= expect_true("session baseline does not force save",
                    save_result.should_save == 0u);
  return ok;
}

bool validate_session_handle_bookkeeping() {
  OctarynServerPlayerState state = default_state();
  void *session = octaryn_server_player_session_create(&state, 1u);

  bool ok = true;
  ok &= expect_true("session handle create", session != nullptr);
  ok &= expect_true("session handle loaded flag",
                    octaryn_server_player_session_loaded_from_save(session) ==
                        1u);

  OctarynServerPlayerState current{};
  ok &= expect_true("session handle state read",
                    octaryn_server_player_session_state(session, &current) ==
                        0);
  ok &= expect_close("session handle state y", current.y, state.y);

  OctarynServerPlayerSessionSaveResult save_result{};
  ok &= expect_true("session handle unchanged decision",
                    octaryn_server_player_session_handle_save_decision(
                        session, 1.5, 1u, &save_result) == 0);
  ok &= expect_true("session handle unchanged save flag",
                    save_result.should_save == 0u);
  ok &= expect_true("session handle note saved",
                    octaryn_server_player_session_handle_note_saved(
                        session, &save_result.save_state) == 0);

  ok &= expect_true("session handle rejects null state",
                    octaryn_server_player_session_state(nullptr, &current) !=
                        0);
  ok &= expect_true("session handle rejects null save result",
                    octaryn_server_player_session_handle_save_decision(
                        session, 0.0, 0u, nullptr) != 0);
  octaryn_server_player_session_destroy(session);
  octaryn_server_player_session_destroy(nullptr);

  ok &= expect_true("session handle rejects null create",
                    octaryn_server_player_session_create(nullptr, 0u) ==
                        nullptr);
  return ok;
}
