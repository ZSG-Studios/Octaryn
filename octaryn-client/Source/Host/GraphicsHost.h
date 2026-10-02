#pragma once
#include "octaryn_host_api.h"
struct SDL_Window;
struct runtime_controls;
namespace octaryn::client::rendering {struct WorldRenderer;}
namespace octaryn::client::host {
struct GraphicsHost {
  SDL_Window* window{};
  runtime_controls* controls{};
  rendering::WorldRenderer* renderer{};
};
int graphics_get(const GraphicsHost*,octaryn_host_graphics_settings*);
int graphics_apply(GraphicsHost*,const octaryn_host_graphics_settings*,unsigned persist);
}
