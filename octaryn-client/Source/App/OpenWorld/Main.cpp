#include "OpenWorld.h"
#include "MainMenu.h"
#include "RenderDistance.h"

#include <SDL3/SDL.h>
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
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  std::puts("octaryn_client_starting=1");
    octaryn::client::app::WorldRunOptions options;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--validate-session-rejoin") == 0) {
            options.validate_session_rejoin = true;
            continue;
        }
    if (std::strcmp(argv[index], "--diagnostic") == 0) {
      options.frame_limit = 180;
      continue;
    }    if (std::strcmp(argv[index], "--frames") == 0 && index + 1 < argc) {
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
    } else if (std::strcmp(argv[index], "--benchmark-streaming-speed") == 0 && index + 1 < argc) {
      char* end=nullptr;
      const double value=std::strtod(argv[++index],&end);
      if(end==argv[index] || *end!='\0' || !std::isfinite(value) || value<1 || value>120) {
        std::fputs("--benchmark-streaming-speed requires metres per second from 1 to 120\n",stderr);return 2;
      }
      options.benchmark_streaming_speed=value;
    } else if (std::strcmp(argv[index], "--benchmark-settings") == 0) {
      options.benchmark_settings=true;
    } else if (std::strcmp(argv[index], "--benchmark-hidden") == 0) {
      options.benchmark_hidden=true;
    } else if (std::strcmp(argv[index], "--validate-distance-changes") == 0) {
      options.validate_distance_changes=true;
    } else if (std::strcmp(argv[index], "--validate-ui") == 0) {
      options.validate_ui=true;
    } else if (std::strcmp(argv[index], "--validate-world-items") == 0) {
      options.validate_world_items=true;
    } else if (std::strcmp(argv[index], "--validate-temporal") == 0) {
      options.validate_temporal=true;
    } else if (std::strcmp(argv[index], "--validate-lighting-motion") == 0) {
      options.validate_lighting_motion=true;
    } else if (std::strcmp(argv[index], "--validate-lighting-edits") == 0) {
      options.validate_lighting_edits=true;
    } else if (std::strcmp(argv[index], "--render-distance") == 0 && index + 1 < argc) {
      char* end=nullptr;
      const long value=std::strtol(argv[++index],&end,10);
      bool supported=false;
      for(int i=0;i<render_distance_option_count();++i)supported|=value==render_distance_options()[i];
      if(end==argv[index] || *end!='\0' || !supported) {
        std::fprintf(stderr,"--render-distance requires one of 4, 8, 12, 16, 20, 24, 32\n");return 2;
      }
      options.render_distance=static_cast<int>(value);
    } else if (std::strcmp(argv[index], "--show-diagnostics") == 0) {
      options.show_diagnostics=true;
    } else if (std::strcmp(argv[index], "--third-person") == 0) {
      options.third_person=true;
    } else if (std::strcmp(argv[index], "--shoulder") == 0 && index + 1 < argc) {
      const char* side=argv[++index];
      if(std::strcmp(side,"left")==0)options.shoulder=octaryn::client::app::CameraShoulder::Left;
      else if(std::strcmp(side,"right")==0)options.shoulder=octaryn::client::app::CameraShoulder::Right;
      else {std::fprintf(stderr,"--shoulder requires left or right\n");return 2;}
      options.third_person=true;
    } else if (std::strcmp(argv[index], "--show-lighting") == 0) {
      options.show_lighting=true;
    } else if (std::strcmp(argv[index], "--show-inventory") == 0) {
      options.show_inventory=true;
    } else if (std::strcmp(argv[index], "--show-creative") == 0) {
      options.show_creative=true;
    } else if (std::strcmp(argv[index], "--show-menu") == 0) {
      options.show_menu=true;
    } else if (std::strcmp(argv[index], "--show-fsr-settings") == 0) {
      options.show_fsr_settings = true;
    } else if (std::strcmp(argv[index], "--capture-ui") == 0 && index + 1 < argc) {
      const char* name=argv[++index];
      bool valid=name[0]!='\0';
      for(const char* c=name;*c;++c)
        valid&=(*c>='a'&&*c<='z')||(*c>='A'&&*c<='Z')||(*c>='0'&&*c<='9')||*c=='-'||*c=='_';
      if(!valid || std::strlen(name)>64) {
        std::fprintf(stderr,"--capture-ui requires a name of up to 64 letters, digits, dashes or underscores\n");return 2;
      }
      options.capture_ui=name;
    } else if (std::strcmp(argv[index], "--show-settings") == 0) {
      options.show_settings = true;
    } else if (std::strcmp(argv[index], "--play-world") == 0 && index + 1 < argc) {
      char* end = nullptr;
      const long value = std::strtol(argv[++index], &end, 10);
      if (end == argv[index] || *end != '\0' || value < 1 || value > 3) {
        std::fprintf(stderr, "--play-world requires a world slot from 1 to 3\n");
        return 2;
      }
      options.play_world_slot = static_cast<unsigned>(value);
    } else if ((std::strcmp(argv[index], "--connect") == 0 && index + 1 < argc) ||
               std::strncmp(argv[index], "--connect=", 10) == 0) {
      const char* value = std::strcmp(argv[index], "--connect") == 0 ? argv[++index] : argv[index] + 10;
      if (!normalize_connect_endpoint(value, options.connect_endpoint)) {
        std::fprintf(stderr, "--connect requires [host:]port with port 1-65535\n");
        return 2;
      }
    } else {
      std::fprintf(stderr, "Usage: Octaryn.Client [--diagnostic | --frames count | --benchmark-seconds duration] [--benchmark-settings] [--benchmark-hidden] [--show-settings] [--show-inventory | --show-creative | --show-menu] [--third-person] [--shoulder left|right] [--render-distance chunks] [--show-lighting] [--show-diagnostics] [--capture-ui name] [--play-world slot] [--connect [host:]port] [--validate-ui] [--validate-distance-changes] [--validate-world-items] [--validate-temporal]\n");
      return 2;
    }
  }
    if (options.validate_session_rejoin) {
        const char* world = SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
        if (!world || !*world || (!options.play_world_slot && options.connect_endpoint.empty()) ||
            options.frame_limit || options.benchmark_seconds > 0 || options.validate_ui ||
            options.validate_world_items || options.validate_temporal || options.validate_distance_changes ||
            options.validate_lighting_motion || options.validate_lighting_edits) {
            std::fputs("--validate-session-rejoin requires an isolated OCTARYN_CLIENT_WORLD_PATH and --play-world or --connect without other qualification modes\n", stderr);
            return 2;
        }
    }
    if(options.benchmark_streaming_speed>0 && (options.benchmark_seconds<=0 || options.frame_limit ||
      options.validate_distance_changes || options.validate_ui || options.validate_world_items || options.validate_temporal)) {
    std::fputs("--benchmark-streaming-speed requires --benchmark-seconds without other validation/frame limits\n",stderr);return 2;
  }
  if((options.benchmark_settings || (options.benchmark_hidden && !options.validate_ui && !options.validate_world_items && !options.validate_temporal)) && options.benchmark_seconds<=0) {
    std::fprintf(stderr,"--benchmark-settings requires --benchmark-seconds; --benchmark-hidden also supports explicit UI/item/temporal validation\n");return 2;
  }
  if(options.validate_distance_changes) {
    if(options.render_distance && options.render_distance!=4) {
      std::fprintf(stderr,"--validate-distance-changes starts at render distance 4\n");return 2;
    }
    options.render_distance=4;
  }
  if(options.validate_world_items) {
    const char* world=SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
    const char* capture=SDL_getenv("OCTARYN_CLIENT_CAPTURE_PATH");
    if(!world||!*world||!capture||!*capture||options.frame_limit||options.benchmark_seconds>0||
        options.validate_distance_changes||options.validate_ui||options.validate_temporal) {
      std::fprintf(stderr,"--validate-world-items requires explicit isolated OCTARYN_CLIENT_WORLD_PATH and OCTARYN_CLIENT_CAPTURE_PATH, without other validation/frame/benchmark limits\n");
      return 2;
    }
    options.render_distance=4;
  }
  if(options.validate_temporal) {
    const char* world=SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
    const char* capture=SDL_getenv("OCTARYN_CLIENT_CAPTURE_PATH");
    const char* override_mode=SDL_getenv("OCTARYN_CLIENT_UPSCALER");
    if(!world||!*world||!capture||!*capture||options.frame_limit||options.benchmark_seconds>0||
        options.validate_world_items||options.validate_distance_changes||options.validate_ui||
        (override_mode&&*override_mode)) {
      std::fprintf(stderr,"--validate-temporal requires isolated OCTARYN_CLIENT_WORLD_PATH and OCTARYN_CLIENT_CAPTURE_PATH, without OCTARYN_CLIENT_UPSCALER or other validation/frame/benchmark limits\n");
      return 2;
    }
    options.render_distance=4;
  }
  if(options.validate_lighting_motion || options.validate_lighting_edits) {
    const char* world=SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
    const char* capture=SDL_getenv("OCTARYN_CLIENT_CAPTURE_PATH");
    if(!world||!*world||!capture||!*capture||options.frame_limit<600||!options.validate_ui||
        options.benchmark_seconds>0||options.validate_temporal||options.validate_world_items||options.validate_distance_changes||
        (options.validate_lighting_motion && options.validate_lighting_edits)) {
      std::fputs("Lighting validation requires isolated world/capture paths, --validate-ui and --frames >=600 without other validation modes\n",stderr);
      return 2;
    }
    options.render_distance=4;
  }
  if(options.play_world_slot>0 && !octaryn::client::app::menu_boot_requested(options)) {
    std::fprintf(stderr,"--play-world loads through the main menu without benchmark/validation/frame/show flags\n");return 2;
  }
  return octaryn::client::app::run_open_world(options);
}
