#include "ModuleHost.h"

#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)

#include "ActionAudio.h"
#include "GameUi.h"
#include "HostExports.h"

#include <SDL3/SDL_timer.h>

#include <cstdio>
#include <cstring>

namespace octaryn::client::host {
namespace {

ModuleHostHooks s_hooks;
octaryn_host_input_snapshot s_last_input{};
uint64_t s_frame_index{};
bool s_ticked{};

double OCTARYN_ABI_CALL time_now_seconds() {
  return static_cast<double>(SDL_GetTicksNS()) / 1e9;
}

uint64_t OCTARYN_ABI_CALL time_tick_id() { return s_frame_index; }

double OCTARYN_ABI_CALL time_tick_rate() { return 60.0; }

void OCTARYN_ABI_CALL diagnostics_log_write(uint32_t level, const char* message) {
  static const char* const names[] = {"trace", "debug", "info", "warning", "error"};
  std::printf("module_api level=%s %s\n",
      level <= 4u ? names[level] : "unknown", message != nullptr ? message : "");
  std::fflush(stdout);
}

int OCTARYN_ABI_CALL input_poll(octaryn_host_input_snapshot* out_snapshot) {
  if (out_snapshot == nullptr || !s_ticked) return 1;
  *out_snapshot = s_last_input;
  return 0;
}

int OCTARYN_ABI_CALL audio_play_action_sound(
    uint64_t asset_id_hash, float volume, float x, float y, float z) {
  (void)volume;
  (void)x;
  (void)y;
  (void)z;
  if (s_hooks.audio == nullptr) return -1;
  static const audio::ActionSound sounds[] = {
      audio::ActionSound::Place, audio::ActionSound::Break,
      audio::ActionSound::Select, audio::ActionSound::Change};
  const auto sound = sounds[asset_id_hash % 4u];
  switch (play_action_audio(s_hooks.audio, sound)) {
    case audio::PlayResult::Played: return 0;
    case audio::PlayResult::Busy: return 2;
    default: return -1;
  }
}

int OCTARYN_ABI_CALL ui_show_notification(const char* text_utf8) {
  if (s_hooks.ui == nullptr || text_utf8 == nullptr) return -1;
  s_hooks.ui->show_notification(text_utf8);
  return 0;
}

int OCTARYN_ABI_CALL ui_poll_action(char* buffer, uint32_t capacity) {
  (void)buffer;
  (void)capacity;
  return 1; // No module-declared UI actions yet.
}

int OCTARYN_ABI_CALL enqueue_command(octaryn_host_command* command) {
  // Module commands from the client have no consumer yet; report failure so
  // modules see the drop instead of a silent hole.
  (void)command;
  return 0;
}

const octaryn_host_time_api s_time_api = {
    OCTARYN_HOST_TIME_API_VERSION, OCTARYN_HOST_TIME_API_SIZE,
    time_now_seconds, time_tick_id, time_tick_rate};
const octaryn_host_diagnostics_api s_diagnostics_api = {
    OCTARYN_HOST_DIAGNOSTICS_API_VERSION, OCTARYN_HOST_DIAGNOSTICS_API_SIZE,
    diagnostics_log_write};
const octaryn_host_input_api s_input_api = {
    OCTARYN_HOST_INPUT_API_VERSION, OCTARYN_HOST_INPUT_API_SIZE, input_poll};
const octaryn_host_audio_api s_audio_api = {
    OCTARYN_HOST_AUDIO_API_VERSION, OCTARYN_HOST_AUDIO_API_SIZE,
    audio_play_action_sound};
const octaryn_host_ui_api s_ui_api = {
    OCTARYN_HOST_UI_API_VERSION, OCTARYN_HOST_UI_API_SIZE,
    ui_show_notification, ui_poll_action};

const void* OCTARYN_ABI_CALL query_host_api(uint32_t api_id, uint32_t min_version) {
  switch (api_id) {
    case OCTARYN_HOST_API_TIME:
      return min_version <= OCTARYN_HOST_TIME_API_VERSION ? &s_time_api : nullptr;
    case OCTARYN_HOST_API_DIAGNOSTICS:
      return min_version <= OCTARYN_HOST_DIAGNOSTICS_API_VERSION ? &s_diagnostics_api : nullptr;
    case OCTARYN_HOST_API_INPUT:
      return min_version <= OCTARYN_HOST_INPUT_API_VERSION ? &s_input_api : nullptr;
    case OCTARYN_HOST_API_AUDIO:
      return min_version <= OCTARYN_HOST_AUDIO_API_VERSION ? &s_audio_api : nullptr;
    case OCTARYN_HOST_API_UI:
      return min_version <= OCTARYN_HOST_UI_API_VERSION ? &s_ui_api : nullptr;
    default:
      return nullptr;
  }
}

bool s_started{};

} // namespace

int module_host_start(const ModuleHostHooks& hooks) {
  if (s_started) module_host_stop();
  s_hooks = hooks;
  s_ticked = false;

  octaryn_client_native_host_api api{};
  api.version = 1u;
  api.size = OCTARYN_CLIENT_NATIVE_HOST_API_SIZE;
  api.enqueue_command = enqueue_command;
  api.query_host_api = query_host_api;

  const int result = octaryn_client_initialize(&api);
  if (result != 0) {
    std::fprintf(stderr, "module_host_start failed=%d\n", result);
    return result;
  }

  s_started = true;
  std::printf("module_host active=1 audio=%u ui=%u\n",
      hooks.audio != nullptr ? 1u : 0u, hooks.ui != nullptr ? 1u : 0u);
  return 0;
}

int module_host_tick(uint64_t frame_index, double delta_seconds,
                     const octaryn_host_input_snapshot& input) {
  if (!s_started) return 1;
  s_frame_index = frame_index;
  s_last_input = input;
  s_ticked = true;

  octaryn_host_frame_snapshot frame{};
  frame.version = 1u;
  frame.size = OCTARYN_HOST_FRAME_SNAPSHOT_SIZE;
  frame.input = input;
  frame.timing.version = 1u;
  frame.timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
  frame.timing.frame_index = frame_index;
  frame.timing.delta_seconds = delta_seconds;
  return octaryn_client_tick(&frame);
}

void module_host_stop() {
  if (!s_started) return;
  octaryn_client_shutdown();
  s_started = false;
  s_hooks = {};
}

} // namespace octaryn::client::host

#else

namespace octaryn::client::host {

int module_host_start(const ModuleHostHooks&) { return 1; }

int module_host_tick(uint64_t, double, const octaryn_host_input_snapshot&) { return 1; }

void module_host_stop() {}

} // namespace octaryn::client::host

#endif
