#include "Environment.h"

#include "Log.h"
#include "AssetPath.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <limits>
#include <string>

namespace octaryn_client_app {

uint32_t read_exit_after_frames() {
  const char *value = std::getenv("OCTARYN_CLIENT_APP_EXIT_AFTER_FRAMES");
  if (value == nullptr || value[0] == '\0') {
    return 0;
  }

  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (end == value) {
    return 0;
  }

  return parsed > std::numeric_limits<uint32_t>::max()
             ? std::numeric_limits<uint32_t>::max()
             : static_cast<uint32_t>(parsed);
}

bool read_enabled_flag(const char *name) {
  const char *value = std::getenv(name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

bool env_value_is_present(const char *name) {
  const char *value = std::getenv(name);
  return value != nullptr && value[0] != '\0';
}

bool set_process_env(const char *name, const std::filesystem::path &value) {
  const std::string text = value.string();
  return SDL_setenv_unsafe(name, text.c_str(), 1) == 0;
}

bool set_process_env_text(const char *name, const char *value) {
  return SDL_setenv_unsafe(name, value, 1) == 0;
}

bool build_client_bundle_path(char *path, size_t path_size,
                              const char *relative_path,
                              const char *failure_label) {
  if (!bundle_path_build(path, path_size, relative_path)) {
    log_line(failure_label);
    return false;
  }
  return true;
}

} // namespace octaryn_client_app
