#pragma once
#include "LightingSettings.h"
#include <filesystem>

struct SDL_Window;

namespace octaryn::client::app {

// Panel-free lighting state: authored defaults from the lighting settings
// contract, a session debug view, and JSON persistence (boot load, save on
// exit or change) in the user preferences directory.
class LightingState {
public:
  explicit LightingState(SDL_Window* window);
  LightingState(const LightingState&) = delete;
  LightingState& operator=(const LightingState&) = delete;
  void save();
  lighting_settings values = lighting_settings_default_value();
  unsigned debug_view{}; // Session-only renderer view from lighting_debug_views.
private:
  std::filesystem::path path_;
};

}
