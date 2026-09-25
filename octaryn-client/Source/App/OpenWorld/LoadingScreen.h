#pragma once

struct SDL_Window;

namespace octaryn::client::app {

// Pumps pending OS messages and labels the window during blocking boot stages
// (renderer creation, map load) so the app never looks hung before the first
// frame. Call between stages, never during an RHI frame.
void pump_boot_stage(SDL_Window* window, const char* stage);

}
