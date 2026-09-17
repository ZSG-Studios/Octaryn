#pragma once
#include <string>

struct SDL_Window;
struct runtime_controls;

namespace octaryn::client::app {
class GameUi;
// Pumps pending OS messages and labels the window during blocking boot stages
// (renderer creation, server spawn) so the app never looks hung before the
// first menu frame. Call between stages, never during an RHI frame.
void pump_boot_stage(SDL_Window* window, const char* stage);
struct LoadingProgress {
  float fraction{};
  std::string status;
  std::string detail;
  bool ready{};
};
// Maps session warmup (authoritative pose, resident columns, mesh backlog)
// onto a 0..1 loading bar. No LOD: progress waits for every visible column.
LoadingProgress loading_progress(
    bool pose_ready,
    unsigned columns,
    unsigned expected_columns,
    unsigned pending_meshes,
    const std::string& session_status);
// Pushes one warmup sample to the loading overlay. Closes the menu and
// retitles the window once the world is fully resident. Returns true when
// loading just completed.
bool update_world_loading(
    GameUi& ui,
    runtime_controls& controls,
    SDL_Window* window,
    bool pose_ready,
    unsigned columns,
    unsigned radius,
    unsigned pending_meshes,
    const std::string& session_status);
}
