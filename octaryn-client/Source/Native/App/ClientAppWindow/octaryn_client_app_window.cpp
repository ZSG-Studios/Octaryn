#include "octaryn_client_app_window.h"

#include "octaryn_client_app_log.h"

namespace octaryn_client_app {

bool window_output_size(SDL_Window *window, int *width, int *height) {
  if (!SDL_GetWindowSizeInPixels(window, width, height)) {
    log_line("window_output_size=failed");
    return false;
  }

  if (*width <= 0 || *height <= 0) {
    log_line("window_output_size=invalid");
    return false;
  }

  return true;
}

} // namespace octaryn_client_app
