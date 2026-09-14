#include "SessionIo.h"
#include "SessionFiles.h"
#include "SessionIoWait.h"
#include <glaze/glaze.hpp>
#include <chrono>
#include <cmath>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

namespace octaryn::client::app::local_session {
struct PlayerStateFile {
  int version{};
  std::string source;
  uint64_t frameIndex{}, sourceTick{};
  double sourceSeconds{std::numeric_limits<double>::quiet_NaN()};
  float playerX{}, playerY{}, playerZ{}, playerPitch{}, playerYaw{};
  float playerVelocityX{}, playerVelocityY{}, playerVelocityZ{};
  uint32_t playerControlMode{}, playerOnGround{};
  float worldTimeDayFraction{};
  double worldTimeTotalSeconds{};
};
namespace {
using Clock = std::chrono::steady_clock;
std::optional<LocalPlayerPose> parse_pose(std::string_view text) {
  PlayerStateFile file;
  constexpr glz::opts options{.error_on_unknown_keys = false};
  if (glz::read<options>(file, text) || file.version != 1 || file.source != "server_player_state_stream" ||
      !std::isfinite(file.sourceSeconds) || file.sourceSeconds < 0 ||
      !std::isfinite(file.worldTimeDayFraction) || file.worldTimeDayFraction < 0 || file.worldTimeDayFraction >= 1 ||
      !std::isfinite(file.worldTimeTotalSeconds) || file.worldTimeTotalSeconds < 0 ||
      !std::isfinite(file.playerX) || !std::isfinite(file.playerY) || !std::isfinite(file.playerZ) ||
      !std::isfinite(file.playerPitch) || !std::isfinite(file.playerYaw) ||
      !std::isfinite(file.playerVelocityX) || !std::isfinite(file.playerVelocityY) || !std::isfinite(file.playerVelocityZ)) return {};
  return LocalPlayerPose{file.playerX, file.playerY, file.playerZ, file.playerYaw, file.playerPitch,
      file.playerVelocityX, file.playerVelocityY, file.playerVelocityZ,
      file.playerOnGround != 0, file.playerControlMode == 1, file.sourceSeconds, file.sourceTick,
      file.worldTimeDayFraction, file.worldTimeTotalSeconds};
}
}

struct SessionIo::State {
  struct Input { std::string text; Clock::time_point submitted; };
  std::filesystem::path pose_path, input_path, window_path, interaction_path;
  std::mutex mutex;
  SessionIoWait wait;
  std::thread thread;
  bool stopped{}, edit_timed_out{};
  std::optional<Input> input;
  std::optional<std::string> window;
  std::deque<std::string> edits;
  size_t queued_edits{};
  Update update;

  void run() {
    std::string payload, previous_payload;
    std::optional<LocalPlayerPose> previous_pose;
    std::optional<std::string> pending_window, pending_edit;
    bool edit_inflight = false;
    bool timed_out = false;
    Clock::time_point edit_sent{};
    auto next = Clock::now();
    while (true) {
      if (!wait.until(next)) break;
      {
        std::lock_guard lock(mutex);
        if (stopped) break;
      }
      if (read_text(pose_path, payload) && payload != previous_payload) {
        if (const auto pose = parse_pose(payload)) {
          previous_payload = payload;
          if (!previous_pose || (pose->source_tick > previous_pose->source_tick &&
                                pose->source_seconds > previous_pose->source_seconds)) {
            previous_pose = pose;
            std::lock_guard lock(mutex);
            update.pose = pose;
          }
        }
      }
      std::optional<Input> outgoing;
      {
        std::lock_guard lock(mutex);
        if (stopped) break;
        outgoing.swap(input);
        if (window) { pending_window.swap(window); window.reset(); }
        if (!pending_edit && !edit_inflight && !edits.empty()) {
          pending_edit = std::move(edits.front());
          edits.pop_front();
        }
      }
      std::string io_status;
      // Never refresh a main-thread input during a stall or replay a failed write.
      if (outgoing && Clock::now() - outgoing->submitted < std::chrono::milliseconds(250) &&
          !write_text(input_path, outgoing->text)) io_status = "Input write failed";
      if (pending_window) {
        if (write_text(window_path, *pending_window)) pending_window.reset();
        else io_status = "Chunk request write failed";
      }
      std::string interaction_status;
      if (edit_inflight || pending_edit) {
        std::error_code error;
        const bool pending = std::filesystem::exists(interaction_path, error);
        if (error) interaction_status = "Cannot check server interaction acknowledgement";
        else if (pending) {
          if (!edit_inflight) edit_sent = Clock::now();
          edit_inflight = true;
          const auto wait = Clock::now() - edit_sent;
          if (wait > std::chrono::seconds(2)) interaction_status = "Waiting for server to consume block command";
          if (wait > std::chrono::seconds(10)) {
            timed_out = true;
            interaction_status = "Block command acknowledgement timed out; command retained";
          }
        } else {
          edit_inflight = false;
          timed_out = false;
          if (pending_edit) {
            if (write_text(interaction_path, *pending_edit)) {
              pending_edit.reset();
              {
                std::lock_guard lock(mutex);
                --queued_edits;
              }
              edit_inflight = true;
              edit_sent = Clock::now();
            } else interaction_status = "Block command write failed; command retained";
          }
        }
      }
      {
        std::lock_guard lock(mutex);
        update.status = std::move(io_status);
        update.interaction_status = std::move(interaction_status);
        edit_timed_out = timed_out;
      }
      // Keep the60Hz phase while skipping missed polls after slow I/O.
      next = SessionIoWait::next(next, Clock::now());
    }
  }
};

SessionIo::SessionIo(std::filesystem::path pose, std::filesystem::path input,
    std::filesystem::path window, std::filesystem::path interaction) : state_(std::make_unique<State>()) {
  state_->pose_path = std::move(pose);
  state_->input_path = std::move(input);
  state_->window_path = std::move(window);
  state_->interaction_path = std::move(interaction);
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
bool SessionIo::submit_edit(std::string text) {
  std::lock_guard lock(state_->mutex);
  if (state_->stopped || state_->edit_timed_out || state_->queued_edits >= 64) return false;
  state_->edits.push_back(std::move(text));
  ++state_->queued_edits;
  return true;
}
}
