#include "LoadingScreen.h"

#include <SDL3/SDL.h>
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
    std::string title = "ZSG Engine | ";
    title += stage;
    SDL_SetWindowTitle(window, title.c_str());
  }
  SDL_PumpEvents();
}

}
