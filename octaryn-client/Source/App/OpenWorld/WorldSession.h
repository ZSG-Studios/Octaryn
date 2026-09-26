#pragma once
#include "ActionAudio.h"
#include <filesystem>

struct SDL_Window;

namespace octaryn::client::rendering { struct WorldRenderer; }
namespace octaryn::client::world_presentation { class BlockInteraction; }

namespace octaryn::client::app {
struct WorldRunOptions;
struct WorldControls;
class GameUi;
class LightingPanel;
class LocalSession;
class StreamingBenchmark;
class WorldProfile;

// Persistent state shared with one world session. Everything the frame loop
// mutates crosses by reference; per-session objects (stream, validations,
// poses) stay local to run_world_session so a later session starts clean.
struct WorldSession {
  SDL_Window* window{};
  const WorldRunOptions* options{};
  WorldProfile* profile{};
  StreamingBenchmark* streaming{};
  LightingPanel* lighting{};
  WorldControls* controls{};
  unsigned* radius{};
  const std::filesystem::path* world{};
  int* width{};
  int* height{};
  rendering::WorldRenderer* renderer{};
  world_presentation::BlockInteraction* interaction{};
  audio::ActionAudioOwner* audio{};
  GameUi* ui{};
  bool show_loading{};
  bool remote_authority{}; // True when the session streams from a dedicated server.
};
struct SessionOutcome {
  bool disconnect{};
  int code{};
};
// Runs one authoritative session until quit, disconnect, or completion.
// Disconnect (pause-menu return) stops cleanly for a later menu phase.
SessionOutcome run_world_session(WorldSession& session, LocalSession& authority);
// Mesh-map session: same authority contract, GLB world presentation, voxel
// gameplay surfaces removed.
SessionOutcome run_map_world_session(WorldSession& session, LocalSession& authority);
}
