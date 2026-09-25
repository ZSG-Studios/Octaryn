#include "OpenWorld.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  std::puts("zsg_engine_client_starting=1");
  octaryn::client::app::WorldRunOptions options;
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--diagnostic") == 0) {
      options.frame_limit = 180;
    } else if (std::strcmp(argv[index], "--frames") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const long value = std::strtol(argv[++index], &end, 10);
      if (end == argv[index] || *end != '\0' || value <= 0 || value > 1000000) {
        std::fprintf(stderr, "--frames requires an integer from 1 to 1000000\n");
        return 2;
      }
      options.frame_limit = static_cast<int>(value);
    } else if (std::strcmp(argv[index], "--benchmark-seconds") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const double value = std::strtod(argv[++index], &end);
      if (end == argv[index] || *end != '\0' || !std::isfinite(value) || value < 1 || value > 3600) {
        std::fprintf(stderr, "--benchmark-seconds requires a duration from 1 to 3600\n");
        return 2;
      }
      options.benchmark_seconds = value;
    } else if (std::strcmp(argv[index], "--benchmark-settings") == 0) {
      options.benchmark_settings = true;
    } else if (std::strcmp(argv[index], "--benchmark-hidden") == 0) {
      options.benchmark_hidden = true;
    } else if (std::strcmp(argv[index], "--validate-frame-pacing") == 0) {
      options.validate_frame_pacing = true;
    } else if (std::strcmp(argv[index], "--third-person") == 0) {
      options.third_person = true;
    } else if (std::strcmp(argv[index], "--shoulder") == 0 && index + 1 < argc) {
      const char* side = argv[++index];
      if (std::strcmp(side, "left") == 0) options.shoulder = octaryn::client::app::CameraShoulder::Left;
      else if (std::strcmp(side, "right") != 0) { std::fprintf(stderr, "--shoulder requires left or right\n"); return 2; }
      options.third_person = true;
    } else if (std::strcmp(argv[index], "--show-diagnostics") == 0) {
      options.show_diagnostics = true;
    } else if (std::strcmp(argv[index], "--capture-ui") == 0 && index + 1 < argc) {
      const char* name = argv[++index];
      bool valid = name[0] != '\0';
      for (const char* c = name; *c; ++c)
        valid &= (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_';
      if (!valid || std::strlen(name) > 64) {
        std::fprintf(stderr, "--capture-ui requires a name of up to 64 letters, digits, dashes or underscores\n");
        return 2;
      }
      options.capture_ui = name;
    } else {
      std::fprintf(stderr, "Usage: Octaryn.Client [--diagnostic | --frames count | --benchmark-seconds duration] "
                           "[--benchmark-settings] [--benchmark-hidden] [--third-person] [--shoulder left|right] "
                           "[--show-diagnostics] [--capture-ui name] [--validate-frame-pacing]\n");
      std::fputs("Frame pacing qualification: --validate-frame-pacing [--frames count] (default 180; uses saved cap/VSync)\n", stderr);
      return 2;
    }
  }
  if (options.validate_frame_pacing) {
    if (options.benchmark_seconds > 0) {
      std::fputs("--validate-frame-pacing supports a standalone frame run without benchmarks\n", stderr);
      return 2;
    }
    if (!options.frame_limit) options.frame_limit = 180;
  }
  if ((options.benchmark_settings || options.benchmark_hidden) && options.benchmark_seconds <= 0) {
    std::fprintf(stderr, "--benchmark-settings and --benchmark-hidden require --benchmark-seconds\n");
    return 2;
  }
  return octaryn::client::app::run_open_world(options);
}
