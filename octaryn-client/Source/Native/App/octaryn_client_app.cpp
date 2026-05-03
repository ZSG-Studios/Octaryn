#include "octaryn_client_host_exports.h"
#include "octaryn_client_window_lifecycle.h"
#include "octaryn_native_crash_diagnostics.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;
constexpr double kDefaultDeltaSeconds = 1.0 / 60.0;
constexpr Uint8 kClearRed = 18;
constexpr Uint8 kClearGreen = 43;
constexpr Uint8 kClearBlue = 49;
constexpr Uint8 kClearAlpha = 255;
constexpr Uint8 kBlockRed = 110;
constexpr Uint8 kBlockGreen = 189;
constexpr Uint8 kBlockBlue = 87;
constexpr int kBlockDrawSize = 48;
constexpr int kMaxPresentationUpdatesPerFrame = 256;

FILE *g_log;

struct presentation_block {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t block;
};

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

bool read_enabled_flag(const char *name) {
  const char *value = std::getenv(name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
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

uint64_t pack_signed_pair(int32_t a, int32_t b) {
  return static_cast<uint32_t>(a) |
         (static_cast<uint64_t>(static_cast<uint32_t>(b)) << 32u);
}

uint64_t pack_block(int32_t z, uint16_t block) {
  return static_cast<uint32_t>(z) | (static_cast<uint64_t>(block) << 32u);
}

int32_t unpack_low(uint64_t value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value));
}

int32_t unpack_high(uint64_t value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value >> 32u));
}

int apply_probe_snapshot() {
  octaryn_replication_change changes[1]{};
  changes[0].version = 1u;
  changes[0].size = OCTARYN_REPLICATION_CHANGE_SIZE;
  changes[0].change_kind = 1u;
  changes[0].replication_id = 1u;
  changes[0].payload0 = pack_signed_pair(0, 0);
  changes[0].payload1 = pack_block(0, 7u);

  octaryn_server_snapshot_header snapshot{};
  snapshot.version = 1u;
  snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  snapshot.change_count = 1u;
  snapshot.tick_id = 1u;
  snapshot.changes_address =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(changes));

  return octaryn_client_apply_server_snapshot(&snapshot);
}

void apply_presentation_update(std::vector<presentation_block> &blocks,
                               const octaryn_replication_change &change) {
  if (change.version != 1u || change.size != OCTARYN_REPLICATION_CHANGE_SIZE ||
      change.change_kind != 1u) {
    return;
  }

  presentation_block update{};
  update.x = unpack_low(change.payload0);
  update.y = unpack_high(change.payload0);
  update.z = unpack_low(change.payload1);
  update.block = static_cast<uint16_t>(change.payload1 >> 32u);

  for (auto iterator = blocks.begin(); iterator != blocks.end(); ++iterator) {
    if (iterator->x == update.x && iterator->y == update.y &&
        iterator->z == update.z) {
      if (update.block == 0u) {
        blocks.erase(iterator);
      } else {
        *iterator = update;
      }
      return;
    }
  }

  if (update.block != 0u) {
    blocks.push_back(update);
  }
}

bool drain_presentation_updates(std::vector<presentation_block> &blocks) {
  octaryn_replication_change changes[kMaxPresentationUpdatesPerFrame]{};
  uint32_t written = 0u;
  const int result = octaryn_client_drain_presentation_updates(
      changes, kMaxPresentationUpdatesPerFrame, &written);
  if (result != 0) {
    log_result("drain_presentation_updates", result);
    return false;
  }

  for (uint32_t index = 0u; index < written; ++index) {
    apply_presentation_update(blocks, changes[index]);
  }

  if (written != 0u && g_log != nullptr) {
    std::fprintf(g_log, "presentation_updates_drained=%" PRIu32 "\n", written);
    std::fflush(g_log);
  }
  return true;
}

bool draw_blocks(SDL_Renderer *renderer,
                 const std::vector<presentation_block> &blocks) {
  if (!SDL_SetRenderDrawColor(renderer, kBlockRed, kBlockGreen, kBlockBlue,
                              kClearAlpha)) {
    log_line("block_draw_color=failed");
    return false;
  }

  for (const presentation_block &block : blocks) {
    const float screen_x =
        static_cast<float>(kWindowWidth / 2 + block.x * kBlockDrawSize +
                           block.z * kBlockDrawSize / 2 -
                           kBlockDrawSize / 2);
    const float screen_y =
        static_cast<float>(kWindowHeight / 2 - block.y * kBlockDrawSize -
                           block.z * kBlockDrawSize / 3 -
                           kBlockDrawSize / 2);
    SDL_FRect rect{screen_x, screen_y, static_cast<float>(kBlockDrawSize),
                   static_cast<float>(kBlockDrawSize)};
    if (!SDL_RenderFillRect(renderer, &rect)) {
      log_line("block_draw=failed");
      return false;
    }
  }

  return true;
}

bool present_frame(SDL_Renderer *renderer,
                   const std::vector<presentation_block> &blocks) {
  if (!SDL_SetRenderDrawColor(renderer, kClearRed, kClearGreen, kClearBlue,
                              kClearAlpha)) {
    log_line("render_color=failed");
    return false;
  }

  if (!SDL_RenderClear(renderer)) {
    log_line("render_clear=failed");
    return false;
  }

  if (!draw_blocks(renderer, blocks)) {
    return false;
  }

  if (!SDL_RenderPresent(renderer)) {
    log_line("render_present=failed");
    return false;
  }

  return true;
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
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                           SDL_WINDOW_HIDDEN);
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

  SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
  if (renderer == nullptr) {
    log_line("renderer_create=failed");
    if (g_log != nullptr) {
      std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 7;
  }
  log_line("renderer_create=0");

  octaryn_client_native_host_api api{};
  api.version = 1u;
  api.size = OCTARYN_CLIENT_NATIVE_HOST_API_SIZE;
  api.enqueue_command = enqueue_command;

  int result = octaryn_client_initialize(&api);
  log_result("initialize", result);
  if (result != 0) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_log != nullptr) {
      std::fclose(g_log);
    }
    return 5;
  }

  if (read_enabled_flag("OCTARYN_CLIENT_APP_PRESENTATION_PROBE_SNAPSHOT")) {
    result = apply_probe_snapshot();
    log_result("presentation_probe_snapshot", result);
    if (result != 0) {
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      if (g_log != nullptr) {
        std::fclose(g_log);
      }
      return 8;
    }
  }

  const uint32_t exit_after_frames = read_exit_after_frames();
  bool running = true;
  uint64_t frame_index = 0u;
  uint64_t previous_ticks = SDL_GetTicksNS();
  std::vector<presentation_block> presentation_blocks;
  while (running) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT ||
          event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
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

    if (!drain_presentation_updates(presentation_blocks)) {
      result = -3;
      running = false;
      break;
    }

    if (!present_frame(renderer, presentation_blocks)) {
      if (g_log != nullptr) {
        std::fprintf(g_log, "sdl_error=%s\n", SDL_GetError());
      }
      result = -2;
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
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (g_log != nullptr) {
    std::fclose(g_log);
  }

  return result == 0 ? 0 : 6;
}
