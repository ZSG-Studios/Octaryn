#include "LoadingScreen.h"
#include "GameUi.h"
#include "RuntimeControls.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <string>

namespace octaryn::client::app {

void pump_boot_stage(SDL_Window* window, const char* stage)
{
  if (!stage) stage = "loading";
  static Uint64 boot_start = 0;
  if (!boot_start) boot_start = SDL_GetTicksNS();
  std::printf("client_boot stage=%s elapsed_ms=%.0f\n",
      stage, double(SDL_GetTicksNS() - boot_start) / 1e6);
  std::fflush(stdout);
  if (window)
  {
    std::string title = "Octaryn | ";
    title += stage;
    SDL_SetWindowTitle(window, title.c_str());
  }
  SDL_PumpEvents();
}

LoadingProgress loading_progress(
    bool pose_ready,
    unsigned columns,
    unsigned expected_columns,
    unsigned pending_meshes,
    const std::string& session_status)
{
  LoadingProgress progress;
  if (!pose_ready)
  {
    progress.fraction = 0.08f;
    progress.status = "Starting authoritative server...";
    progress.detail = session_status;
    return progress;
  }
  const unsigned expected = expected_columns > 0 ? expected_columns : 1;
  const float resident = std::clamp(
      static_cast<float>(columns) / static_cast<float>(expected), 0.0f, 1.0f);
  if (columns < expected)
  {
    progress.fraction = 0.1f + resident * 0.65f;
    progress.status = "Streaming world columns...";
    progress.detail = std::to_string(columns) + " / " + std::to_string(expected) + " columns resident";
    return progress;
  }
  if (pending_meshes > 0)
  {
    progress.fraction = 0.85f;
    progress.status = "Meshing terrain...";
    progress.detail = std::to_string(pending_meshes) + " meshes pending";
    return progress;
  }
  progress.fraction = 1.0f;
  progress.status = "World ready";
  progress.ready = true;
  return progress;
}

bool update_world_loading(
    GameUi& ui,
    runtime_controls& controls,
    SDL_Window* window,
    bool pose_ready,
    unsigned columns,
    unsigned radius,
    unsigned pending_meshes,
    const std::string& session_status)
{
  const unsigned width = 2 * radius + 1;
  const auto progress = loading_progress(
      pose_ready, columns, width * width, pending_meshes, session_status);
  ui.update_loading(progress.status, progress.detail, progress.fraction);
  if (!progress.ready) return false;
  ui.hide_loading();
  display_menu_close(&controls.display_menu);
  if (window) SDL_SetWindowTitle(window, "Octaryn");
  return true;
}
}
