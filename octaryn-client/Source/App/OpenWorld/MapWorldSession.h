#pragma once

#include "WorldRenderer.h"
#include <SDL3/SDL.h>

namespace octaryn::client::app {

struct WorldRunOptions;
class WorldProfile;
struct WorldControls;
class LocalSession;
class DebugOverlay;
class LightingState;
struct MapManifest;

struct MapSessionContext {
  SDL_Window* window{};
  const WorldRunOptions* options{};
  WorldProfile* profile{};
  WorldControls* controls{};
  LocalSession* session{};
  DebugOverlay* overlay{};
  LightingState* lighting{};
  const MapManifest* manifest{};
  bool remote_authority{};
  bool show_loading{};
  int* width{};
  int* height{};
  rendering::WorldRenderer* renderer{};
};

struct SessionOutcome {
  bool disconnect{};
  int code{};
};

// Mesh-map session: the bundled dedicated server owns the player pose and
// world time; the client renders and sends input intents. No voxel streaming,
// block edits, selection or inventory exist.
class LocalSession;
SessionOutcome run_map_world_session(MapSessionContext& ctx, LocalSession& session);

}
