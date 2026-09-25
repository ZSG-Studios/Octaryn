#pragma once
#include "PlayerControlInput.h"
#include <cstdint>

namespace octaryn::client::app {

// Standalone first-person player state for mesh map worlds. The engine owns
// the pose directly; no authoritative server session exists.
struct MapPlayer {
  float x{}, y{}, z{};       // Eye position in map space.
  float yaw{}, pitch{};
  float velocity_x{}, velocity_y{}, velocity_z{};
  bool flying{true};
  float world_day_fraction{0.5f}; // Noon start, matching the previous world clock.
  double source_seconds{};
};

// Speeds match the previous authoritative fly-through presentation.
void map_player_spawn(MapPlayer& player, float x, float y, float z, float yaw, float pitch);
void map_player_update(MapPlayer& player, const player_control_input& input,
                       bool flying, float yaw, float pitch, double elapsed_seconds);
void map_player_step_hours(MapPlayer& player, int hours);

}
