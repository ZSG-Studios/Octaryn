#pragma once

struct SDL_Window;
namespace octaryn::client::rendering {struct WorldRenderer;}

namespace octaryn::client::app {
// Returns after map loading has stopped using the renderer, including on close.
bool start_map(SDL_Window* window, rendering::WorldRenderer* renderer,
    const char* glb_path, bool& running);
}
