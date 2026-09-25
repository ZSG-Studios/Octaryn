#pragma once

struct SDL_Window;
namespace octaryn::client::rendering {struct WorldRenderer;}
namespace octaryn::client::app {class LocalSession;}

namespace octaryn::client::app {
// Returns after map loading has stopped using the renderer, including on close.
// Player collision for client-side prediction is armed before returning.
bool start_map(SDL_Window* window, rendering::WorldRenderer* renderer,
    const char* glb_path, bool& running, LocalSession& session);
}
