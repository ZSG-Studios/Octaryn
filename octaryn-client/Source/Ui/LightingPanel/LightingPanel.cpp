#include "LightingPanel.h"
#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace octaryn::client::app {
struct SavedLighting {
  int version{1};
  float ambient_strength{.82f},sun_strength{1},sun_fallback_strength{1};
  float fog_distance{256},skylight_floor{.08f};
};
LightingPanel::LightingPanel(SDL_Window*) {
  if (const char* debug=SDL_getenv("OCTARYN_CLIENT_LIGHTING_DEBUG"))
    debug_view=static_cast<unsigned>(std::clamp(std::atoi(debug),0,27));
  if (const char* override_path=SDL_getenv("OCTARYN_CLIENT_LIGHTING_PATH"))
    path_=std::filesystem::path(reinterpret_cast<const char8_t*>(override_path));
  else {
    char* directory=SDL_GetPrefPath("ZSGStudios","Octaryn");
    if (!directory) throw std::runtime_error(SDL_GetError());
    path_=std::filesystem::path(reinterpret_cast<const char8_t*>(directory))/"lighting-settings.json";
    SDL_free(directory);
  }
  std::ifstream input(path_,std::ios::binary);
  if (input) {
    std::string text(std::istreambuf_iterator<char>{input},{});
    SavedLighting saved;
    if (text.size()<16384 && !glz::read<glz::opts{.error_on_unknown_keys=false}>(saved,text) && saved.version==1) {
      values={1,saved.fog_distance,saved.skylight_floor,saved.ambient_strength,saved.sun_strength,saved.sun_fallback_strength};
      lighting_settings_sanitize(&values);
    }
  }
}
void LightingPanel::save() {
  const SavedLighting saved{1,values.ambient_strength,values.sun_strength,
      values.sun_fallback_strength,values.fog_distance,values.skylight_floor};
  const auto json=glz::write_json(saved);
  if (!json) return;
  std::error_code error;std::filesystem::create_directories(path_.parent_path(),error);
  std::ofstream output(path_,std::ios::binary|std::ios::trunc);
  if (!output || !output.write(json->data(),static_cast<std::streamsize>(json->size())))
    std::fprintf(stderr,"Lighting settings save failed\n");
}
}
