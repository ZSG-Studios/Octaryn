#include "LocalSession.h"
#include "Prediction.h"
#include "PredictionWorld.h"
#include "PoseHistory.h"
#include "ServerProcess.h"
#include "SessionFiles.h"
#include "SessionIo.h"
#include "BlockInteraction.h"
#include "MapMode.h"
#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)
#include "HostExports.h"
#endif
#include <glaze/glaze.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>

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
struct BlockInteractionFile {
  int version{1};
  uint64_t frameIndex{}, movementFrameID{};
  std::vector<BlockCommandFile> commands;
};
}

struct LocalSession::State {
  local_session::ServerProcess process;
  local_session::PoseHistory history;
  std::filesystem::path root, runtime, snapshot, stream, input, pose, chunk_intent, shutdown;
  std::filesystem::path interaction;
  std::unique_ptr<local_session::SessionIo> io;
  std::string interaction_status;
  uint64_t edit_sequence{}, sent_input_frame{};
 BlockReceipts receipts;
 uint64_t receipt_ack{};
  int time_hour_offset{};
  std::string status{"stopped"};
  uint64_t epoch{};
  local_session::Prediction prediction;
 std::unique_ptr<local_session::PredictionWorld> collision_world;
  uint32_t radius{4}, published_radius{};
  int32_t center_x{}, center_z{};
  bool benchmark_center{};
  int32_t benchmark_x{},benchmark_z{};
  double send_elapsed{}, age{}, pose_age{};
  bool started{}, remote{};
  std::string endpoint;
};

namespace {
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
  state.snapshot = state.runtime / "chunk_stream.json";
  state.stream = state.runtime / "chunk_stream.json.bin";
  state.input = state.runtime / "player_input.json";
  state.pose = state.runtime / "player_state.json";
  state.chunk_intent = state.runtime / "chunk_view.json";
  state.shutdown = state.runtime / "shutdown.request";
  state.interaction = state.runtime / "block_interaction.json";
  state.radius = std::clamp(radius, 1u, 32u);
  logs = log_root.empty() ? state.root.parent_path().parent_path() / "logs" / "server"
                          : std::filesystem::absolute(log_root);
  std::filesystem::create_directories(state.runtime);
  std::filesystem::create_directories(logs);
  for (const auto& path : {state.snapshot, state.stream, state.input, state.pose, state.shutdown, state.interaction, state.runtime / "block_results.json", state.runtime / "block_results_ack.json", state.runtime / "world_time.json"}) {
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error) { state.status = "Cannot clear previous session files"; return false; }
  }
  if (!publish_window(state, 0, 0)) { state.status = "Cannot write initial chunk request"; return false; }
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
    const std::filesystem::path& world_root, uint32_t radius, const std::filesystem::path& log_root) {
  stop();
  state_ = std::make_unique<State>();
  auto& state = *state_;
  try {
    std::filesystem::path logs;
    if (!prepare_runtime(state, world_root, radius, log_root, logs)) return false;
 state.collision_world=std::make_unique<local_session::PredictionWorld>(state.stream,client_bundle);
 state.prediction.set_collision(local_session::PredictionWorld::query,state.collision_world.get());
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
      {"OCTARYN_SERVER_PROCESS_STREAM_INTERVAL_MS", "16"},
      {"OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY", "1"},
      {"OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH", ""},
      {"OCTARYN_CLIENT_DISABLE_GAME_MODULES", "0"},
      {"OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY", "0"},
      {"OCTARYN_SERVER_WORLD_BLOCKS_PATH", utf8_path(state.root / "world_blocks.json")},
      {"OCTARYN_SERVER_PLAYER_SAVE_ROOT", utf8_path(state.root)},
      {"OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH", utf8_path(state.chunk_intent)},
      {"OCTARYN_SERVER_CHUNK_STREAM_PATH", utf8_path(state.snapshot)},
      {"OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH", utf8_path(state.input)},
      {"OCTARYN_SERVER_PLAYER_STATE_STREAM_PATH", utf8_path(state.pose)},
      {"OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH", utf8_path(state.shutdown)},
      {"OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH", utf8_path(state.interaction)},
      {"OCTARYN_SERVER_WORLD_TIME_INTENT_PATH", utf8_path(state.runtime / "world_time.json")}};
    const auto map_manifest_path = client_bundle / "Assets" / "Maps" / "map.json";
    const char* map_override = std::getenv("OCTARYN_CLIENT_MAP_MODE");
    const bool map_forced_off = map_override && *map_override == '0';
    MapManifest map_manifest;
    if (!map_forced_off && load_map_manifest(client_bundle, map_manifest)) {
      environment.emplace_back("OCTARYN_SERVER_MAP_MODE", "1");
      environment.emplace_back("OCTARYN_SERVER_MAP_PATH", utf8_path(map_manifest.glb));
      environment.emplace_back("OCTARYN_SERVER_MAP_MANIFEST_PATH", utf8_path(map_manifest_path));
    }
    if (!state.process.start(executable, logs / "local-session.log", environment)) {
      state.status = "Could not start packaged server";
      return false;
    }
    state.started = true;
    state.io = std::make_unique<local_session::SessionIo>(state.pose, state.input, state.chunk_intent, state.interaction);
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
 state.collision_world=std::make_unique<local_session::PredictionWorld>(state.stream,client_bundle);
 state.prediction.set_collision(local_session::PredictionWorld::query,state.collision_world.get());
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
    state.io = std::make_unique<local_session::SessionIo>(state.pose, state.input, state.chunk_intent, state.interaction);
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
  const auto received = state.io->poll();
  state.interaction_status = received.interaction_status;
 if (received.receipts) {
 if (state.receipts.session!=received.receipts->session) state.receipt_ack=0;
 state.receipts=*received.receipts;
 }
  state.age += elapsed_seconds;
  state.send_elapsed += elapsed_seconds;
  state.pose_age += elapsed_seconds;
  if (received.pose && state.history.push(*received.pose)) {
 state.pose_age = 0;
 state.prediction.reconcile(*received.pose,received.acknowledged_input_frame);
 }
 if (state.collision_world && !state.history.empty()) {
 LocalPlayerPose current=state.history.latest();
 state.prediction.sample(current);
 state.collision_world->update(current.x,current.z);
 }
 state.prediction.advance(input,elapsed_seconds,state.pose_age);
  state.history.advance(elapsed_seconds);
  if (state.history.empty()) {
    if (state.age > 30) state.status = "Waiting for authoritative player state";
    return;
  }
  state.status = state.pose_age > 1.0 ? "Waiting for server; holding last pose"
      : (state.remote ? "Connected to remote server" : "Connected to local server");
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

bool LocalSession::submit_block_edit(const world_presentation::BlockEditIntent& edit, uint64_t* command_id) {
  auto& state = *state_;
  if (!running() || state.history.empty() || state.pose_age > 1.0 || !state.io) {
    state.interaction_status = "Block command not queued: server unavailable or command queue full";
    return false;
  }
  if (!std::isfinite(edit.camera_x) || !std::isfinite(edit.camera_y) || !std::isfinite(edit.camera_z) ||
      edit.edit.y < -256 || edit.edit.y >= 256) return false;
  local_session::BlockInteractionFile file;
  file.frameIndex = state.edit_sequence + 1;
  file.movementFrameID=state.sent_input_frame;
  file.commands.push_back({file.frameIndex, edit.edit.x, edit.edit.y, edit.edit.z, edit.block,
      edit.camera_x, edit.camera_y, edit.camera_z, edit.hit.x, edit.hit.y, edit.hit.z});
  std::string text;
  if (glz::write_json(file, text) || !state.io->submit_edit(std::move(text))) {
    state.interaction_status = "Block command not queued: acknowledgement timed out or command queue full";
    return false;
  }
  state.edit_sequence = file.frameIndex;
 if (command_id) *command_id = file.frameIndex;
  return true;
}

const BlockReceipts& LocalSession::block_receipts() const { return state_->receipts; }
bool LocalSession::acknowledge_block_receipts(const std::string& session,uint64_t sequence) {
 auto& state=*state_;
 if (!state.io || session!=state.receipts.session || sequence<state.receipt_ack ||
 state.receipts.receipts.empty() || sequence>state.receipts.receipts.back().sequence) return false;
 if (sequence==state.receipt_ack) return true;
 std::string text;
 if (glz::write_json(BlockReceiptAck{1,session,sequence},text)) return false;
 state.io->publish_receipt_ack(std::move(text));
 state.receipt_ack=sequence;
 return true;
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
 state.collision_world.reset();
 state.history = {};
 state.receipts = {}; state.receipt_ack=0;
 state.started = false;
  state.status = "Stopped";
}
bool LocalSession::running() const { return state_->started && session_alive(*state_); }
bool LocalSession::player_pose(LocalPlayerPose& pose) const { return state_->prediction.sample(pose); }
LocalMovementStats LocalSession::movement_stats() const { return state_->prediction.stats(); }
void LocalSession::set_collision_query(CollisionQuery query,void* context) { state_->prediction.set_collision(query,context); }
void LocalSession::set_radius(uint32_t radius) { state_->radius = std::clamp(radius, 1u, 32u); }
const std::filesystem::path& LocalSession::chunk_stream_path() const { return state_->stream; }
const std::string& LocalSession::status() const {
  return state_->interaction_status.empty() ? state_->status : state_->interaction_status;
}
}
