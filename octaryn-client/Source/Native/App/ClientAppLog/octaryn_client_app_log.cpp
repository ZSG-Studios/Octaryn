#include "octaryn_client_app_log.h"

#include <cstdio>
#include <cstdlib>

namespace octaryn_client_app {

FILE *g_log = nullptr;

void open_log() {
  const char *log_path = std::getenv("OCTARYN_CLIENT_APP_LOG_PATH");
  if (log_path != nullptr && log_path[0] != '\0') {
    g_log = std::fopen(log_path, "w");
  }
}

void close_log() {
  if (g_log != nullptr) {
    std::fclose(g_log);
    g_log = nullptr;
  }
}

void log_line(const char *message) {
  if (g_log != nullptr) {
    std::fprintf(g_log, "%s\n", message);
    std::fflush(g_log);
  }
}

void log_result(const char *name, int result) {
  if (g_log != nullptr) {
    std::fprintf(g_log, "%s=%d\n", name, result);
    std::fflush(g_log);
  }
}

} // namespace octaryn_client_app
