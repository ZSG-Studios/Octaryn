#pragma once

struct SDL_Window;
struct runtime_controls;
namespace octaryn::client::rendering {struct WorldRenderer;}

namespace octaryn::client::app {
// Returns only after the worker has stopped accessing the window/renderer.
rendering::WorldRenderer* start_renderer(SDL_Window* window, bool& running,
    const runtime_controls& settings);
}
