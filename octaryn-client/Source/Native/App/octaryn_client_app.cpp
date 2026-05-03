#include "octaryn_client_host_exports.h"
#include "octaryn_client_window_lifecycle.h"
#include "octaryn_native_crash_diagnostics.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;

FILE *g_log;

void log_line(const char *message) {
  if (g_log != nullptr) {
    std::fprintf(g_log, "%s\n", message);
    std::fflush(g_log);
  }
}

void log_result(const char *name, int result) {
  if (g_log != nullptr) {
    std::fprintf(g_log, "%s=%d\n", name, result);
    std::fflush(g_log);
  }
}

uint32_t read_exit_after_frames() {
  const char *value = std::getenv("OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES");
  if (value == nullptr || value[0] == '\0') {
    return 0;
  }

  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (end == value) {
    return 0;
  }

  return parsed > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(parsed);
}

int OCTARYN_ABI_CALL enqueue_command(octaryn_host_command *command) {
  if (g_log != nullptr && command != nullptr) {
    std::fprintf(g_log,
                 "enqueue_command kind=%" PRIu32 " request=%" PRIu64 "\n",
                 command->kind, command->request_id);
    std::fflush(g_log);
  }

  return 1;
}

octaryn_host_frame_snapshot create_frame(uint64_t frame_index,
                                         double delta_seconds) {
  octaryn_host_frame_snapshot frame{};
  frame.version = 1u;
  frame.size = OCTARYN_HOST_FRAME_SNAPSHOT_SIZE;
  frame.input.version = 1u;
  frame.input.size = OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
  frame.timing.version = 1u;
  frame.timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
  frame.timing.frame_index = frame_index;
  frame.timing.delta_seconds = delta_seconds;
  return frame;
}

double frame_delta_seconds(uint64_t previous_ticks, uint64_t current_ticks) {
  if (previous_ticks == 0u || current_ticks <= previous_ticks) {
    return kDefaultDeltaSeconds;
  }

  return static_cast<double>(current_ticks - previous_ticks) / 1000000000.0;
}

void open_log() {
  const char *log_path = std::getenv("OCTARYN_CLIENT_APP_LOG_PATH");
  if (log_path != nullptr && log_path[0] != '\0') {
    g_log = std::fopen(log_path, "w");
  }
}

} // namespace

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  open_log();
  octaryn_native_crash_diagnostics_init("octaryn-client-app");
  if (g_log != nullptr) {
    std::fprintf(g_log, "crash_marker=%s\n",
                 octaryn_native_crash_diagnostics_marker_path());
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    log_line("sdl_init=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      std::fclose(g_log);
    }
    return 2;
  }

  SDL_Window *window =
      SDL_CreateWindow("Octaryn", kWindowWidth, kWindowHeight,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
  if (window == nullptr) {
    log_line("window_create=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 3;
  }

  if (!octaryn_client_window_lifecycle_show(window)) {
    log_line("window_show=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 4;
  }
  octaryn_client_window_lifecycle_finish_show(window);
  log_line("window_show=0");

  octaryn_client_native_host_api api{};
  api.version = 1u;
  api.size = OCTARYN_CLIENT_NATIVE_HOST_API_SIZE;
  api.enqueue_command = enqueue_command;

  int result = octaryn_client_initialize(&api);
  log_result("initialize", result);
  if (result != 0) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 5;
  }

  const uint32_t exit_after_frames = read_exit_after_frames();
  bool running = true;
  uint64_t frame_index = 0u;
  uint64_t previous_ticks = SDL_GetTicksNS();
  while (running) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      }
    }

    const uint64_t current_ticks = SDL_GetTicksNS();
    octaryn_host_frame_snapshot frame = create_frame(
        frame_index + 1u, frame_delta_seconds(previous_ticks, current_ticks));
    previous_ticks = current_ticks;

    result = octaryn_client_tick(&frame);
    log_result("tick", result);
    if (result != 0) {
      running = false;
      break;
    }

    ++frame_index;
    if (exit_after_frames != 0u && frame_index >= exit_after_frames) {
      running = false;
    }

    SDL_Delay(1u);
  }

  octaryn_client_shutdown();
  log_line("shutdown=0");
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (g_log != nullptr) {
    std::fclose(g_log);
  }

  return result == 0 ? 0 : 6;
}
