#include "OpenWorld.h"
#include "MainMenu.h"
#include "../Startup/AppClock.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

namespace {
bool normalize_connect_endpoint(const char* value, std::string& endpoint) {
  if (!value || !*value) return false;
  std::string text(value);
  std::string host, port;
  if (!text.empty() && text[0] == '[') {
    const auto bracket = text.find(']');
    if (bracket == std::string::npos || bracket + 2 > text.size() || text[bracket + 1] != ':') return false;
    host = text.substr(1, bracket - 1);
    port = text.substr(bracket + 2);
  } else {
    const auto separator = text.find_last_of(':');
    if (separator == std::string::npos) {
      host = "127.0.0.1";
      port = text;
    } else {
      if (separator == 0) return false;
      host = text.substr(0, separator);
      port = text.substr(separator + 1);
    }
  }
  if (host.empty() || port.empty()) return false;
  char* end = nullptr;
  const long number = std::strtol(port.c_str(), &end, 10);
  if (end == port.c_str() || *end != '\0' || number < 1 || number > 65535) return false;
  endpoint = host + ":" + std::to_string(number);
  return true;
}
} // namespace


int main(int argc, char** argv) {
  octaryn::client::app::start_app_clock();
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  std::puts("zsg_engine_client_starting=1");
  octaryn::client::app::WorldRunOptions options;
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--validate-map-switches") == 0 && index + 2 < argc) {
      options.map_switch_worlds[0] = argv[++index];
      options.map_switch_worlds[1] = argv[++index];
      continue;
    }
    if (std::strcmp(argv[index], "--validate-session-rejoin") == 0) {
      options.validate_session_rejoin = true;
      continue;
    }
    if (std::strcmp(argv[index], "--diagnostic") == 0) {
      options.frame_limit = 180;
      continue;
    }
    if (std::strcmp(argv[index], "--frames") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const long value = std::strtol(argv[++index], &end, 10);
      if (end == argv[index] || *end != '\0' || value <= 0 || value > 1000000) {
        std::fprintf(stderr, "--frames requires an integer from 1 to 1000000\n");
        return 2;
      }
      options.frame_limit = static_cast<int>(value);
      continue;
    }
    if (std::strcmp(argv[index], "--benchmark-seconds") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const double value = std::strtod(argv[++index], &end);
      if (end == argv[index] || *end != '\0' || !(value == value && value < 1e30 && value > -1e30) || value < 1 || value > 3600) {
        std::fprintf(stderr, "--benchmark-seconds requires a duration from 1 to 3600\n");
        return 2;
      }
      options.benchmark_seconds = value;
      continue;
    }
    if (std::strcmp(argv[index], "--benchmark-settings") == 0) {
      options.benchmark_settings = true;
      continue;
    }
    if (std::strcmp(argv[index], "--benchmark-hidden") == 0) {
      options.benchmark_hidden = true;
      continue;
    }
    if (std::strcmp(argv[index], "--validate-frame-pacing") == 0) {
      options.validate_frame_pacing = true;
      continue;
    }
    if (std::strcmp(argv[index], "--validate-ui") == 0) {
      options.validate_ui = true;
      continue;
    }
    if (std::strcmp(argv[index], "--show-item-target") == 0) {
      options.show_item_target = true;
      continue;
    }
    if (std::strcmp(argv[index], "--validate-module-actions") == 0) {
      options.validate_module_actions = true;
      continue;
    }
    if (std::strcmp(argv[index], "--validate-temporal") == 0) {
      options.validate_temporal = true;
      continue;
    }
    if (std::strcmp(argv[index], "--show-settings") == 0) {
      options.show_settings = true;
      continue;
    }
    if (std::strcmp(argv[index], "--show-fsr-settings") == 0) {
      options.show_fsr_settings = true;
      continue;
    }
    if (std::strcmp(argv[index], "--show-menu") == 0) {
      options.show_menu = true;
      continue;
    }
    if (std::strcmp(argv[index], "--show-inventory") == 0) {
      options.show_inventory = true;
      continue;
    }
    if (std::strcmp(argv[index], "--show-creative") == 0) {
      options.show_creative = true;
      continue;
    }
    if (std::strcmp(argv[index], "--show-diagnostics") == 0) {
      options.show_diagnostics = true;
      continue;
    }
    if (std::strcmp(argv[index], "--show-lighting") == 0) {
      options.show_lighting = true;
      continue;
    }
    if (std::strcmp(argv[index], "--third-person") == 0) {
      options.third_person = true;
      continue;
    }
    if (std::strcmp(argv[index], "--shoulder") == 0 && index + 1 < argc) {
      const char* side = argv[++index];
      if (std::strcmp(side, "left") == 0) options.shoulder = octaryn::client::app::CameraShoulder::Left;
      else if (std::strcmp(side, "right") != 0) { std::fprintf(stderr, "--shoulder requires left or right\n"); return 2; }
      options.third_person = true;
      continue;
    }
    if (std::strcmp(argv[index], "--capture-ui") == 0 && index + 1 < argc) {
      const char* name = argv[++index];
      bool valid = name[0] != '\0';
      for (const char* c = name; *c; ++c) {
        const bool ok = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
                        (*c >= '0' && *c <= '9') || *c == '-' || *c == '_';
        valid = valid && ok;
      }
      if (!valid || std::strlen(name) > 64) {
        std::fprintf(stderr, "--capture-ui requires a name of up to 64 letters, digits, dashes or underscores\n");
        return 2;
      }
      options.capture_ui = name;
      continue;
    }
    if (std::strcmp(argv[index], "--play-world") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const long value = std::strtol(argv[++index], &end, 10);
      if (end == argv[index] || *end != '\0' || value < 1 || value > 3) {
        std::fprintf(stderr, "--play-world requires a slot from 1 to 3\n");
        return 2;
      }
      options.play_world_slot = static_cast<unsigned>(value);
      continue;
    }
    if ((std::strcmp(argv[index], "--connect") == 0 && index + 1 < argc) ||
        std::strncmp(argv[index], "--connect=", 10) == 0) {
      const char* value = std::strcmp(argv[index], "--connect") == 0 ? argv[++index] : argv[index] + 10;
      if (!normalize_connect_endpoint(value, options.connect_endpoint)) {
        std::fprintf(stderr, "--connect requires [host:]port with port 1-65535\n");
        return 2;
      }
      continue;
    }
    std::fprintf(stderr, "Usage: Octaryn.Client [--diagnostic | --frames count | --benchmark-seconds duration] "
                         "[--benchmark-settings] [--benchmark-hidden] [--show-settings | --show-fsr-settings | --show-menu | --show-lighting] "
                         "[--third-person] [--shoulder left|right] [--show-diagnostics] [--show-item-target] [--capture-ui name] [--play-world slot] [--connect [host:]port] [--validate-frame-pacing] [--validate-module-actions]\n");
    std::fputs("Frame pacing qualification: --validate-frame-pacing [--frames count] (default 180; uses saved cap/VSync)\n", stderr);
    return 2;
  }
  if (options.validate_frame_pacing) {
    if (options.benchmark_seconds > 0 || options.validate_session_rejoin) {
      std::fputs("--validate-frame-pacing supports a standalone frame run without benchmarks\n", stderr);
      return 2;
    }
    if (!options.frame_limit) options.frame_limit = 180;
  }
  if (options.validate_session_rejoin) {
    if (!options.play_world_slot && options.connect_endpoint.empty()) {
      std::fputs("--validate-session-rejoin requires --play-world or --connect\n", stderr);
      return 2;
    }
  }
  if (options.validate_module_actions && !options.frame_limit) options.frame_limit=360;
  const bool any_validation = options.validate_ui || options.validate_temporal || options.validate_module_actions ||
      options.validate_frame_pacing || options.validate_session_rejoin || !options.map_switch_worlds[0].empty() ||
      options.validate_world_items || options.validate_block_actions ||
      options.validate_distance_changes || options.validate_lighting_motion ||
      options.validate_lighting_edits;
  if ((options.benchmark_settings || options.benchmark_hidden) && options.benchmark_seconds <= 0 &&
      !any_validation && !options.frame_limit) {
    std::fprintf(stderr, "--benchmark-settings requires --benchmark-seconds; --benchmark-hidden also supports explicit UI validation\n");
    return 2;
  }
  return octaryn::client::app::run_open_world(options);
}
