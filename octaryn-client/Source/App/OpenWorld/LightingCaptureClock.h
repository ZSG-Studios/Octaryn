#pragma once
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace octaryn::client::app {
// Isolate presentation comparisons without changing authoritative world time.
class LightingCaptureClock {
public:
  LightingCaptureClock(bool hidden,double benchmark_seconds) {
    const auto* value=SDL_getenv("OCTARYN_CLIENT_CAPTURE_FIXED_HOUR");
    if(!value)return;
    if(!hidden || benchmark_seconds<=0 || !SDL_getenv("OCTARYN_CLIENT_CAPTURE_PATH"))
      throw std::runtime_error("Fixed lighting clock requires a hidden benchmark capture");
    char* end{};const double hour=std::strtod(value,&end);
    if(end==value || *end || !std::isfinite(hour) || hour<0 || hour>=24)
      throw std::runtime_error("Capture fixed hour must be finite and in [0,24)");
    fixed_=true;day_=hour/24;
    std::printf("lighting_capture_clock fixed=1 hour=%.9f presentation_seconds=0 authority=unchanged\n",hour);
  }
  double day(double live)const{return fixed_?day_:live;}
  double seconds(double live)const{return fixed_?0:live;}
private:
  double day_{};
  bool fixed_{};
};
}
