#pragma once
#include <string>

struct SDL_Window;
struct runtime_controls;
namespace octaryn::client::rendering {struct WorldRenderer;}

namespace octaryn::client::app {
class GameUi;
struct WorldControls;
// Main-thread presentation; the caller must hold exclusive renderer ownership.
bool present_loading(SDL_Window*,octaryn::client::rendering::WorldRenderer*,GameUi&,
    WorldControls&,const std::string& stage,const std::string& detail,bool cancelable,bool draw=true);
bool validate_loading_cancel(SDL_Window*,GameUi&,const std::string& stage);
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
// Map worlds load the GLB payload instead of streaming columns; the bar waits
// for the authoritative pose and the uploaded map.
bool update_map_loading(
    GameUi& ui,
    runtime_controls& controls,
    SDL_Window* window,
    bool pose_ready,
    bool map_ready,
    const std::string& session_status);
}
