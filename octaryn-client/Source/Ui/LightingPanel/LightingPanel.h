#pragma once
#include "LightingSettings.h"
#include "LightingDebugViews.h"
#include <filesystem>
struct SDL_Window;
namespace octaryn::client::app {
class LightingPanel {
public:
  explicit LightingPanel(SDL_Window* window);
  LightingPanel(const LightingPanel&)=delete;
  LightingPanel& operator=(const LightingPanel&)=delete;
  void save();
  bool visible{};
  lighting_settings values=lighting_settings_default_value();
  // Session-only renderer view from lighting_debug_views.
  unsigned debug_view{};
private:
  std::filesystem::path path_;
};
}
