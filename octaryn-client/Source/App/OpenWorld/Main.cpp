#include "OpenWorld.h"
#include "RenderDistance.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>


int main(int argc, char** argv) {
  octaryn::client::app::WorldRunOptions options;
  for (int index = 1; index < argc; ++index) {
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
    } else if (std::strcmp(argv[index], "--show-settings") == 0) {
      options.show_settings = true;
    } else {
      std::fprintf(stderr, "Usage: Octaryn.Client [--diagnostic | --frames count | --benchmark-seconds duration] [--benchmark-settings] [--benchmark-hidden] [--show-settings] [--show-inventory | --show-creative | --show-menu] [--third-person] [--shoulder left|right] [--render-distance chunks] [--show-lighting] [--show-diagnostics] [--validate-ui] [--validate-distance-changes] [--validate-world-items] [--validate-temporal]\n");
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
    const char* world=std::getenv("OCTARYN_CLIENT_WORLD_PATH");
    const char* capture=std::getenv("OCTARYN_CLIENT_CAPTURE_PATH");
    if(!world||!*world||!capture||!*capture||options.frame_limit||options.benchmark_seconds>0||
        options.validate_distance_changes||options.validate_ui||options.validate_temporal) {
      std::fprintf(stderr,"--validate-world-items requires explicit isolated OCTARYN_CLIENT_WORLD_PATH and OCTARYN_CLIENT_CAPTURE_PATH, without other validation/frame/benchmark limits\n");
      return 2;
    }
    options.render_distance=4;
  }
  if(options.validate_temporal) {
    const char* world=std::getenv("OCTARYN_CLIENT_WORLD_PATH");
    const char* capture=std::getenv("OCTARYN_CLIENT_CAPTURE_PATH");
    const char* override_mode=std::getenv("OCTARYN_CLIENT_UPSCALER");
    if(!world||!*world||!capture||!*capture||options.frame_limit||options.benchmark_seconds>0||
        options.validate_world_items||options.validate_distance_changes||options.validate_ui||
        (override_mode&&*override_mode)) {
      std::fprintf(stderr,"--validate-temporal requires isolated OCTARYN_CLIENT_WORLD_PATH and OCTARYN_CLIENT_CAPTURE_PATH, without OCTARYN_CLIENT_UPSCALER or other validation/frame/benchmark limits\n");
      return 2;
    }
    options.render_distance=4;
  }
  if(options.validate_lighting_motion || options.validate_lighting_edits) {
    const char* world=std::getenv("OCTARYN_CLIENT_WORLD_PATH");
    const char* capture=std::getenv("OCTARYN_CLIENT_CAPTURE_PATH");
    if(!world||!*world||!capture||!*capture||options.frame_limit<600||!options.validate_ui||
        options.benchmark_seconds>0||options.validate_temporal||options.validate_world_items||options.validate_distance_changes||
        (options.validate_lighting_motion && options.validate_lighting_edits)) {
      std::fputs("Lighting validation requires isolated world/capture paths, --validate-ui and --frames >=600 without other validation modes\n",stderr);
      return 2;
    }
    options.render_distance=4;
  }
  return octaryn::client::app::run_open_world(options);
}
