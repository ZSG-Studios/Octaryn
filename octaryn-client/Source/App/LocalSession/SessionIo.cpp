#include "SessionIo.h"
#include "SessionFiles.h"
#include "SessionIoWait.h"
#include <glaze/glaze.hpp>
#include <chrono>
#include <deque>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

namespace octaryn::client::app::local_session {
struct PlayerStateFile {
  int version{};
  std::string source;
 uint64_t frameIndex{}, sourceTick{}, acknowledgedInputFrame{};
  double sourceSeconds{std::numeric_limits<double>::quiet_NaN()};
  float playerX{}, playerY{}, playerZ{}, playerPitch{}, playerYaw{};
  float playerVelocityX{}, playerVelocityY{}, playerVelocityZ{};
  uint32_t playerControlMode{}, playerOnGround{};
 uint16_t jumpHeld{};
  float worldTimeDayFraction{};
  double worldTimeTotalSeconds{};
};
struct UiActionAcknowledgement { uint64_t epoch{}, seq{}; };
struct UiActionJournal { int version{1}; uint64_t epoch{}, seq{}; std::vector<std::string> actions; };
struct ModuleEvent { uint64_t seq{}, id{}, kind{}, p1{}, p2{}; };
struct ModuleEventJournal { int version{}; std::vector<ModuleEvent> events; };
namespace {
using Clock = std::chrono::steady_clock;
std::optional<LocalPlayerPose> parse_pose(std::string_view text, uint64_t& acknowledged_input_frame) {
  PlayerStateFile file;
  constexpr glz::opts options{.error_on_unknown_keys = false};
  if (glz::read<options>(file, text) || file.version != 1 || file.source != "server_player_state_stream" ||
      !std::isfinite(file.sourceSeconds) || file.sourceSeconds < 0 ||
      !std::isfinite(file.worldTimeDayFraction) || file.worldTimeDayFraction < 0 || file.worldTimeDayFraction >= 1 ||
      !std::isfinite(file.worldTimeTotalSeconds) || file.worldTimeTotalSeconds < 0 ||
      !std::isfinite(file.playerX) || !std::isfinite(file.playerY) || !std::isfinite(file.playerZ) ||
      !std::isfinite(file.playerPitch) || !std::isfinite(file.playerYaw) ||
      !std::isfinite(file.playerVelocityX) || !std::isfinite(file.playerVelocityY) || !std::isfinite(file.playerVelocityZ)) return {};
 acknowledged_input_frame = file.acknowledgedInputFrame;
 return LocalPlayerPose{file.playerX, file.playerY, file.playerZ, file.playerYaw, file.playerPitch,
      file.playerVelocityX, file.playerVelocityY, file.playerVelocityZ,
      file.playerOnGround != 0, file.playerControlMode == 1, file.sourceSeconds, file.sourceTick,
      file.worldTimeDayFraction, file.worldTimeTotalSeconds, file.jumpHeld != 0};
}
}

struct SessionIo::State {
  struct Input { std::string text; Clock::time_point submitted; };
  std::filesystem::path pose_path, input_path, window_path;
  std::mutex mutex;
  SessionIoWait wait;
  std::thread thread;
  bool stopped{};
  bool read_pose_file{true};
  SessionChannels channels;
  std::optional<Input> input;
  std::optional<std::string> window, time;
  std::deque<std::pair<uint64_t, std::string>> actions;
  std::deque<ModuleEvent> events;
  uint64_t action_sequence{}, received_event{}, consumed_event{};
  const uint64_t action_epoch{static_cast<uint64_t>(Clock::now().time_since_epoch().count())};
  Update update;

  void run() {
    std::string payload, previous_payload;
    std::optional<LocalPlayerPose> previous_pose;
    std::optional<std::string> pending_window, pending_time;
    std::string previous_actions;
    uint64_t published_event_ack{};
    auto next = Clock::now();
    while (true) {
      if (!wait.until(next)) break;
      {
        std::lock_guard lock(mutex);
        if (stopped) break;
      }
      if (read_pose_file && read_text(pose_path, payload) && payload != previous_payload) {
 uint64_t acknowledged_input_frame{};
 if (const auto pose = parse_pose(payload, acknowledged_input_frame)) {
          previous_payload = payload;
          if (!previous_pose || (pose->source_tick > previous_pose->source_tick &&
                                pose->source_seconds > previous_pose->source_seconds)) {
            previous_pose = pose;
            std::lock_guard lock(mutex);
 update.pose = pose;
 update.acknowledged_input_frame = acknowledged_input_frame;
          }
        }
      }
      std::optional<Input> outgoing;
      {
        std::lock_guard lock(mutex);
        if (stopped) break;
        outgoing.swap(input);
        if (time) { pending_time.swap(time); time.reset(); }
        if (window) { pending_window.swap(window); window.reset(); }

      }
      std::string io_status;
      // Never refresh a main-thread input during a stall or replay a failed write.
      if (outgoing && Clock::now() - outgoing->submitted < std::chrono::milliseconds(250) &&
          !write_text(input_path, outgoing->text)) io_status = "Input write failed";
      if (pending_window) {
        if (channels.publish ? channels.publish(1, *pending_window) : write_text(window_path, *pending_window)) pending_window.reset();
        else io_status = "Chunk request write failed";
      }
      if (pending_time) {
        if (channels.publish ? channels.publish(4, *pending_time) : write_text(pose_path.parent_path() / "world_time.json", *pending_time)) pending_time.reset();
        else io_status = "World time request write failed";
      }
      // Cumulative action journals survive worker coalescing and transport delays.
      const auto directory = pose_path.parent_path();
      UiActionAcknowledgement action_ack;
      std::string ack_text;
      if (channels.poll_action_ack) channels.poll_action_ack(action_ack.epoch, action_ack.seq);
      else if (read_text(directory / "ui_action.json.ack", ack_text) && glz::read_json(action_ack, ack_text))
        action_ack = {};
      UiActionJournal journal;
      uint64_t event_ack{};
      {
        std::lock_guard lock(mutex);
        if (action_ack.epoch == action_epoch && action_ack.seq <= action_sequence)
          while (!actions.empty() && actions.front().first <= action_ack.seq) actions.pop_front();
        journal.epoch = action_epoch;
        journal.seq = action_sequence;
        for (const auto& action : actions) journal.actions.push_back(action.second);
        event_ack = consumed_event;
      }
      std::string action_text;
      if (!glz::write_json(journal, action_text) && action_text != previous_actions) {
        if (channels.publish ? channels.publish(5, action_text) : write_text(directory / "ui_action.json", action_text)) previous_actions = action_text;
        else io_status = "UI action write failed";
      }
      std::string event_text;
      if (!channels.publish && read_text(directory / "module_events.json", event_text, 65536)) {
        ModuleEventJournal incoming;
        if (!glz::read_json(incoming, event_text) && incoming.version == 1) {
          std::lock_guard lock(mutex);
          for (const auto& event : incoming.events) {
            if (event.seq <= received_event) continue;
            if (event.seq != received_event + 1 || events.size() >= 256)
              throw std::runtime_error("Module event journal lost ordering or exceeded its bound");
            events.push_back(event);
            received_event = event.seq;
          }
        }
      }
      if (event_ack > published_event_ack) {
        if (write_text(directory / "module_events.json.ack", std::to_string(event_ack))) published_event_ack = event_ack;
        else io_status = "Module event acknowledgement write failed";
      }
      {
        std::lock_guard lock(mutex);
        update.status = std::move(io_status);
      }
 // Keep the60Hz phase while skipping missed polls after slow I/O.
      next = SessionIoWait::next(next, Clock::now());
    }
  }
};

SessionIo::SessionIo(std::filesystem::path pose, std::filesystem::path input,
    std::filesystem::path window, bool read_pose_file, SessionChannels channels) : state_(std::make_unique<State>()) {
  state_->read_pose_file = read_pose_file;
  state_->channels = std::move(channels);
  state_->pose_path = std::move(pose);
  state_->input_path = std::move(input);
  state_->window_path = std::move(window);
  state_->thread = std::thread([state = state_.get()] {
    try { state->run(); }
    catch (const std::exception&) {
      std::lock_guard lock(state->mutex);
      state->stopped = true;
      state->update.status = "Session I/O worker failed; server input will expire";
    }
  });
}
SessionIo::~SessionIo() { stop(); }
void SessionIo::stop() {
  {
    std::lock_guard lock(state_->mutex);
    state_->stopped = true;
  }
  state_->wait.stop();
  if (state_->thread.joinable()) state_->thread.join();
}
SessionIo::Update SessionIo::poll() {
  std::lock_guard lock(state_->mutex);
  Update result = state_->update;
  state_->update.pose.reset();
  return result;
}
void SessionIo::publish_input(std::string text) {
  std::lock_guard lock(state_->mutex);
  if (!state_->stopped) state_->input = State::Input{std::move(text), Clock::now()};
}
void SessionIo::publish_window(std::string text) {
  std::lock_guard lock(state_->mutex);
  if (!state_->stopped) state_->window = std::move(text);
}
void SessionIo::publish_time(std::string text) {
  std::lock_guard lock(state_->mutex);
  if (!state_->stopped) state_->time = std::move(text);
}
bool SessionIo::publish_ui_action(std::string action) {
  std::lock_guard lock(state_->mutex);
  if (state_->stopped || state_->actions.size() >= 256 || action.empty() || action.size() > 128) return false;
  state_->actions.emplace_back(++state_->action_sequence, std::move(action));
  return true;
}
bool SessionIo::poll_module_event(uint64_t& id, uint64_t& kind, uint64_t& p1, uint64_t& p2) {
  std::lock_guard lock(state_->mutex);
  if (state_->events.empty()) return false;
  const auto event = state_->events.front();
  state_->events.pop_front();
  state_->consumed_event = event.seq;
  id = event.id; kind = event.kind; p1 = event.p1; p2 = event.p2;
  return true;
}
}
