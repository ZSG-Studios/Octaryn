#pragma once
#include "ActionAudio.h"
#include <filesystem>
#include <string>
#include <optional>
#include "SceneTransitionHost.h"

struct SDL_Window;

namespace octaryn::client::rendering { struct WorldRenderer; }

namespace octaryn::client::app {
struct WorldRunOptions;
struct WorldControls;
class GameUi;
class LightingPanel;
class LocalSession;
class StreamingBenchmark;
class WorldProfile;
class SceneTransitionRoute;
class FrameTimingLog;

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
  audio::ActionAudioOwner* audio{};
  GameUi* ui{};
  bool show_loading{};
  bool remote_authority{}; // True when the session streams from a dedicated server.
  FrameTimingLog* timing_log{};
  SceneTransitionRoute* transition_route{};
  std::string scene_asset;
  std::uint64_t* module_frame{};
  std::optional<octaryn_host_transition_pose> view_origin;
  std::string transition_error;
};
struct SessionOutcome {
  bool disconnect{};
  int code{};
  std::string loading_error;
  std::optional<host::SceneTransitionRequest> transition;
};
// Runs one authoritative session until quit, disconnect, or completion.
// Disconnect (pause-menu return) stops cleanly for a later menu phase.
SessionOutcome run_world_session(WorldSession& session, LocalSession& authority);
// Mesh-map session: same authority contract, GLB world presentation, voxel
// gameplay surfaces removed.
SessionOutcome run_map_world_session(WorldSession& session, LocalSession& authority);
}
