#include "LocalSession.h"
#include "Prediction.h"
#include "PoseHistory.h"
#include "ServerProcess.h"
#include "SessionFiles.h"
#include "SessionIo.h"
#include "MapManifest.h"
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
#include "HostExports.h"
#include "RemoteSessionExchange.h"
#endif
#include <glaze/glaze.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <vector>

namespace octaryn::client::app {
namespace local_session {
struct TimeIntentFile {
  int version{1};
  int hourOffset{};
};
struct ChunkIntentFile {
  int version{1};
  uint64_t epoch{};
  int32_t centerChunkX{}, centerChunkZ{};
  uint32_t radius{};
  bool hasPreviousWindow{};
  int32_t previousCenterChunkX{}, previousCenterChunkZ{};
  uint32_t previousRadius{};
};
struct BlockCommandFile {
  uint64_t requestId{};
  int32_t editX{}, editY{}, editZ{};
  uint16_t block{};
  float cameraX{}, cameraY{}, cameraZ{};
  int32_t hitX{}, hitY{}, hitZ{};
};
}

struct LocalSession::State {
  local_session::ServerProcess process;
  local_session::PoseHistory history;
  std::filesystem::path root, runtime, snapshot, stream, input, pose, chunk_intent, shutdown;
  std::unique_ptr<local_session::SessionIo> io;
  uint64_t edit_sequence{}, sent_input_frame{};
  uint64_t latest_authority_ack{};
  int time_hour_offset{};
  std::string status{"stopped"};
  uint64_t epoch{};
  local_session::Prediction prediction;
  uint32_t radius{4}, published_radius{};
  int32_t center_x{}, center_z{};
  bool benchmark_center{};
  int32_t benchmark_x{},benchmark_z{};
  double send_elapsed{}, age{}, pose_age{};
  bool started{}, remote{}, loopback{};
  std::string endpoint;
  std::vector<WorldItemPose> world_items = std::vector<WorldItemPose>(10000);
  uint64_t world_items_revision{};
  size_t world_items_count{};
};

namespace {
local_session::SessionChannels session_channels() {
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  return local_session::remote_channels();
#else
  return {};
#endif
}
template <typename SessionState>
bool publish_window(SessionState& state, int32_t x, int32_t z) {
  local_session::ChunkIntentFile intent;
  intent.epoch = state.epoch + 1;
  intent.centerChunkX = x;
  intent.centerChunkZ = z;
  intent.radius = state.radius;
  std::string text;
  if (glz::write_json(intent, text)) return false;
  if (state.io) state.io->publish_window(std::move(text));
  else if (!local_session::write_text(state.chunk_intent, text)) return false;
  state.center_x = x;
  state.center_z = z;
  state.published_radius = state.radius;
  state.epoch = intent.epoch;
  return true;
}

bool prepare_runtime(LocalSession::State& state, const std::filesystem::path& world_root,
    uint32_t radius, const std::filesystem::path& log_root, std::filesystem::path& logs) {
  state.root = std::filesystem::absolute(world_root);
  state.runtime = state.root / "runtime";
  state.input = state.runtime / "player_input.json";
  state.pose = state.runtime / "player_state.json";
  state.chunk_intent = state.runtime / "chunk_view.json";
  state.shutdown = state.runtime / "shutdown.request";
  state.radius = std::clamp(radius, 1u, 32u);
  logs = log_root.empty() ? state.root.parent_path().parent_path() / "logs" / "server"
                          : std::filesystem::absolute(log_root);
  if (const char* directory = std::getenv("OCTARYN_CLIENT_SERVER_LOG_DIR"); directory && *directory)
    logs = std::filesystem::absolute(std::filesystem::u8path(directory));
  std::filesystem::create_directories(state.runtime);
  std::filesystem::create_directories(logs);
  for (const auto& path : {state.input, state.pose, state.shutdown, state.runtime / "world_time.json", state.runtime / "ui_action.json", state.runtime / "ui_action.json.ack", state.runtime / "module_events.json", state.runtime / "module_events.json.ack", state.runtime / "server.endpoint"}) {
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error) { state.status = "Cannot clear previous session files"; return false; }
  }
  return true;
}

bool session_alive(const LocalSession::State& state) {
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  if (state.remote) return octaryn_client_remote_is_running() != 0;
#endif
  return state.process.running();
}

std::string remote_transport_status() {
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  char message[256]{};
  if (octaryn_client_remote_status(message, static_cast<int>(sizeof(message))) >= 0) return message;
#endif
  return "remote transport unavailable";
}

}

LocalSession::LocalSession() : state_(std::make_unique<State>()) {}
LocalSession::~LocalSession() { stop(); }

bool LocalSession::start(const std::filesystem::path& client_bundle,
    const std::filesystem::path& world_root, uint32_t radius, const std::filesystem::path& log_root, const MapManifest* map_override) {
  stop();
  state_ = std::make_unique<State>();
  auto& state = *state_;
#if !defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  state.status = "Bundled authority requires managed transport hosting";
  return false;
#endif
  try {
    std::filesystem::path logs;
    if (!prepare_runtime(state, world_root, radius, log_root, logs)) return false;
      auto executable = std::filesystem::absolute(client_bundle) / "server" /
#if defined(_WIN32)
        "Octaryn.Server.exe";
#else
        "Octaryn.Server";
#endif
    if (!std::filesystem::is_regular_file(executable)) { state.status = "Packaged server executable is missing"; return false; }
    using local_session::utf8_path;
    std::vector<std::pair<std::string, std::string>> environment{
      {"OCTARYN_SERVER_PROCESS_STREAM_LIVE", "1"},
      {"OCTARYN_SERVER_MAP_TRANSFER_SPAWN", map_override && map_override->replace_scene ? "1" : "0"},
      {"OCTARYN_SERVER_MAP_TRANSFER_POSE_PATH", map_override && map_override->replace_scene
          ? utf8_path(map_override->authority_spawn_manifest) : ""},
      {"OCTARYN_SERVER_LISTEN", "127.0.0.1:0"},
      {"OCTARYN_SERVER_WORLD_DIR", utf8_path(state.root)},
      {"OCTARYN_SERVER_SCENE_PHYSICS_DIR", utf8_path(state.runtime)},
      {"OCTARYN_SERVER_LOCAL_ENDPOINT_PATH", utf8_path(state.runtime / "server.endpoint")},
      {"OCTARYN_SERVER_PROCESS_STREAM_INTERVAL_MS", "16"},
      {"OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY", "1"},
      {"OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH", map_override && map_override->replace_scene
          ? utf8_path(state.runtime / ("scene-authority-" + (map_override->authority_spawn_manifest.empty()
              ? map_override->manifest : map_override->authority_spawn_manifest).stem().string() + ".log")) : ""},
      {"OCTARYN_CLIENT_DISABLE_GAME_MODULES", "0"},
      {"OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY", "0"},
      {"OCTARYN_SERVER_WORLD_BLOCKS_PATH", utf8_path(state.root / "world_blocks.json")},
      {"OCTARYN_SERVER_PLAYER_SAVE_ROOT", utf8_path(state.root)},
      {"OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH", ""},
      {"OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH", ""},
      {"OCTARYN_SERVER_UI_ACTION_INTENT_PATH", ""},
      {"OCTARYN_SERVER_MODULE_EVENTS_PATH", ""},
      {"OCTARYN_SERVER_PLAYER_STATE_STREAM_PATH", ""},
      {"OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH", utf8_path(state.shutdown)},
      {"OCTARYN_SERVER_WORLD_TIME_INTENT_PATH", ""}};
    MapManifest map_manifest;
    if (map_override ? (map_manifest=*map_override, true) : load_world_manifest(state.root,client_bundle,map_manifest)) {
      environment.emplace_back("OCTARYN_SERVER_MAP_MODE", "1");
      environment.emplace_back("OCTARYN_SERVER_MAP_PATH", utf8_path(map_manifest.glb));
      environment.emplace_back("OCTARYN_SERVER_MAP_MANIFEST_PATH", utf8_path(map_manifest.manifest));
    } else {
      state.status = "Authoritative map manifest is invalid";
      return false;
    }
    if (!state.process.start(executable, logs / "local-session.log", environment)) {
      state.status = "Could not start packaged server";
      return false;
    }
    state.started = true;
    state.loopback = true;
    state.io = std::make_unique<local_session::SessionIo>(state.pose, state.input, state.chunk_intent, false, session_channels());
    publish_window(state, 0, 0);
    state.status = "Starting authoritative world";
    return true;
  } catch (const std::exception& error) {
    state.status = error.what();
    return false;
  }
}

bool LocalSession::start_remote(const std::filesystem::path& client_bundle,
    const std::filesystem::path& world_root, uint32_t radius, const std::string& endpoint,
    const std::filesystem::path& log_root) {
  (void)client_bundle;
  stop();
  state_ = std::make_unique<State>();
  auto& state = *state_;
  try {
    std::filesystem::path logs;
    if (!prepare_runtime(state, world_root, radius, log_root, logs)) return false;
      state.remote = true;
    state.endpoint = endpoint;
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
    using local_session::utf8_path;
    if (octaryn_client_remote_start(endpoint.c_str(), utf8_path(state.runtime).c_str()) != 0) {
      state.status = std::string("Remote session failed: ") + remote_transport_status();
      octaryn_client_remote_stop();
      return false;
    }
#else
    state.status = "Remote sessions are unavailable in this build";
    return false;
#endif
    state.started = true;
    state.io = std::make_unique<local_session::SessionIo>(state.pose, state.input, state.chunk_intent, false, session_channels());
    publish_window(state, 0, 0);
    state.status = "Connecting to remote server";
    return true;
  } catch (const std::exception& error) {
    state.status = error.what();
    return false;
  }
}

void LocalSession::update(const LocalPlayerInput& input, double elapsed_seconds) {
  auto& state = *state_;
  if (!state.started) return;
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  if (state.loopback && !state.remote) {
    std::string endpoint;
    if (local_session::read_text(state.runtime / "server.endpoint", endpoint, 256)) {
      if (octaryn_client_remote_start_async(endpoint.c_str(), local_session::utf8_path(state.runtime).c_str()) != 0) {
        state.status = "Could not connect to bundled authority";
        return;
      }
      state.remote = true;
    } else if (state.process.running()) {
      return;
    }
  }
#endif
  if (!session_alive(state)) {
    state.status = state.remote ? std::string("Remote server unavailable: ") + remote_transport_status()
                                : "Local server exited; inspect logs/server/local-session.log";
    return;
  }
  if (state.remote) {
    const auto transport = remote_transport_status();
    if (transport.rfind("connected", 0) != 0 && transport.rfind("error", 0) != 0) state.status = transport;
  }
  if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0) return;
  auto received = state.io->poll();
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  if (state.remote) {
    local_session::poll_remote_pose(received);
    const int count = octaryn_client_remote_copy_world_items(&state.world_items_revision,
        state.world_items.data(), static_cast<int>(state.world_items.size()), sizeof(WorldItemPose));
    if (count >= 0) state.world_items_count = static_cast<size_t>(count);
  }
#endif
  state.age += elapsed_seconds;
  state.send_elapsed += elapsed_seconds;
  state.pose_age += elapsed_seconds;
  if (received.pose && state.history.push(*received.pose)) {
    state.latest_authority_ack = received.acknowledged_input_frame;
    state.pose_age = 0;
    state.prediction.reconcile(*received.pose, received.acknowledged_input_frame);
  }

  state.prediction.advance(input, elapsed_seconds, state.pose_age);
  state.history.advance(elapsed_seconds);
  if (state.history.empty()) {
    if (state.age > 30) state.status = "Waiting for authoritative player state";
    return;
  }
  state.status = state.pose_age > 1.0 ? "Waiting for server; holding last pose"
      : (state.remote && !state.loopback ? "Connected to remote server" : "Connected to local server");
  if (!received.status.empty()) state.status = received.status;
  auto pose = state.history.latest();
  state.prediction.sample(pose);
  const auto cx = state.benchmark_center?state.benchmark_x:static_cast<int32_t>(std::floor(pose.x / 32.0f));
  const auto cz = state.benchmark_center?state.benchmark_z:static_cast<int32_t>(std::floor(pose.z / 32.0f));
  if (cx != state.center_x || cz != state.center_z || state.radius != state.published_radius) {
    if (!publish_window(state, cx, cz)) state.status = "Chunk request write failed";
  }
  if (state.send_elapsed < 1.0 / 60.0) return;
  state.send_elapsed = std::fmod(state.send_elapsed, 1.0 / 60.0);
  const auto intent = state.prediction.packet();
  if (intent.commands.empty()) return;
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  if (state.remote) {
    if (!local_session::submit_remote_commands(intent)) state.status = "Invalid remote command batch";
    else state.sent_input_frame = std::max(state.sent_input_frame, intent.commands.back().frameIndex);
    return;
  }
#endif
  std::string text;
  if (glz::write_json(intent, text)) state.status = "Input serialization failed";
  else {
    state.sent_input_frame=std::max(state.sent_input_frame,intent.commands.back().frameIndex);
    state.io->publish_input(std::move(text));
  }
}

void LocalSession::set_benchmark_stream_center(int32_t x,int32_t z) {
  state_->benchmark_center=true;state_->benchmark_x=x;state_->benchmark_z=z;
}

void LocalSession::step_world_hours(int hours) {
  auto& state = *state_;
  if (!running() || !state.io || !hours) return;
  const auto offset = static_cast<int>(std::clamp(static_cast<int64_t>(state.time_hour_offset) + hours,
                                                int64_t{-1000000}, int64_t{1000000}));
  std::string text;
  if (glz::write_json(local_session::TimeIntentFile{1, offset}, text)) return;
  state.time_hour_offset = offset;
  state.io->publish_time(std::move(text));
}

// Module UI actions ride the ui_action intent mailbox. The sequence number
// keeps repeated identical actions distinct for the server's dedup.
bool LocalSession::publish_ui_action(const std::string& action_id) {
  auto& state = *state_;
  if (!running() || !state.io || action_id.empty()) return false;
  if (!state.io->publish_ui_action(action_id)) {
    state.status = "UI action backlog is full";
    return false;
  }
  return true;
}

bool LocalSession::poll_module_event(uint64_t& id, uint64_t& kind, uint64_t& p1, uint64_t& p2) {
  auto& state = *state_;
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  if (state.remote) return octaryn_client_remote_poll_module_event(&id, &kind, &p1, &p2) == 1;
#endif
  return state.io && state.io->poll_module_event(id, kind, p1, p2);
}

bool LocalSession::publish_scene_physics(std::string request) {
  return state_->loopback && running() && state_->io && state_->io->publish_scene_physics(std::move(request));
}
bool LocalSession::scene_physics_snapshot(std::string& snapshot) const {
  return state_->loopback && running() && state_->io && state_->io->scene_physics_snapshot(snapshot);
}

void LocalSession::stop() {
  auto& state = *state_;
  if (state.io) { state.io->stop(); state.io.reset(); }
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
  if (state.remote) octaryn_client_remote_stop();
#endif
  if (state.started && state.process.running()) {
    local_session::write_text(state.shutdown, "stop\n");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (state.process.running() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  state.process.terminate();
  state.prediction = {};
  state.history = {};
  state.started = false;
  state.status = "Stopped";
  state.world_items_count=0;
  state.world_items_revision=0;
}
std::span<const WorldItemPose> LocalSession::world_items() const {
  return {state_->world_items.data(),state_->world_items_count};
}
uint64_t LocalSession::world_items_revision() const { return state_->world_items_revision; }
bool LocalSession::running() const { return state_->started && session_alive(*state_); }
bool LocalSession::player_pose(LocalPlayerPose& pose) const {
  // Predicted body against local Box3D collision when armed; otherwise the
  // Hermite history interpolates authoritative snapshots behind a small fill
  // delay. View angles always stay presentation-local.
  if (state_->prediction.collision_ready()) return state_->prediction.sample(pose);
  if (!state_->history.sample(pose)) return false;
  state_->prediction.view(pose.yaw, pose.pitch);
  return true;
}
bool LocalSession::authority_pose(LocalPlayerPose& pose, std::uint64_t& acknowledged_input) const {
  if(state_->history.empty())return false;
  pose=state_->history.latest();acknowledged_input=state_->latest_authority_ack;return true;
}
bool LocalSession::collision_ready(float x,float y,float z) const {
  return state_->prediction.collision_ready(x,y,z);
}
std::uint64_t LocalSession::last_sent_input_frame() const {return state_->sent_input_frame;}
void LocalSession::set_collision_mesh(const local_session::MeshCollisionSoup& soup) {
  state_->prediction.set_collision(soup);
}
void LocalSession::warm_collision() { state_->prediction.warm_collision(); }
LocalMovementStats LocalSession::movement_stats() const { return state_->prediction.stats(); }
void LocalSession::set_radius(uint32_t radius) { state_->radius = std::clamp(radius, 1u, 32u); }
const std::string& LocalSession::status() const {
  return state_->status;
}
}
