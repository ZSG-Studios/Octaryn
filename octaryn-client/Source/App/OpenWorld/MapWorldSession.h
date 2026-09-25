#pragma once

#include "WorldRenderer.h"
#include <SDL3/SDL.h>

namespace octaryn::client::app {

struct WorldRunOptions;
class WorldProfile;
struct WorldControls;
struct MapPlayer;
class DebugOverlay;
struct LightingState;
struct MapManifest;

struct MapSessionContext {
  SDL_Window* window{};
  const WorldRunOptions* options{};
  WorldProfile* profile{};
  WorldControls* controls{};
  MapPlayer* player{};
  DebugOverlay* overlay{};
  LightingState* lighting{};
  const MapManifest* manifest{};
  int* width{};
  int* height{};
  rendering::WorldRenderer* renderer{};
};

struct SessionOutcome {
  bool disconnect{};
  int code{};
};

// Standalone mesh-map session: renders the GLB map world with a locally
// simulated player. No server process, voxel streaming or authority exist.
SessionOutcome run_map_world_session(MapSessionContext& ctx);

}
