#include "BlockTransportLighting.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace octaryn::client::rendering;
namespace {
using Value=std::array<float,4>;
unsigned checks{};
void require(bool condition,const char* message) {
  ++checks;
  if(!condition) {std::fprintf(stderr,"Block transport lighting: %s\n",message);std::exit(1);}
}
BlockTransportLighting sample(float degrees,float sun=.75f,float ambient=.65f,float visibility=1,float twilight=0) {
  const double angle=double(degrees)*3.14159265358979323846/180;
  return block_transport_lighting({float(std::sin(angle)),float(std::cos(angle)),0,sun},
      {visibility,ambient,twilight,0});
}
void transition_cases() {
  const auto noon=sample(0);
  require(!block_transport_lighting_discontinuity(noon,noon),"stationary sources reset history");
  require(!block_transport_lighting_discontinuity(noon,sample(2.9f)),"small direction change reset history");
  for(float degrees:{3.1f,90.f,180.f,-90.f})
    require(block_transport_lighting_discontinuity(noon,sample(degrees)),"sun direction jump retained stale history");
  require(!block_transport_lighting_discontinuity(noon,sample(0,.8f,.68f,.95f,.04f)),
      "small source/environment variation reset history");
  require(block_transport_lighting_discontinuity(noon,sample(0,1)),"abrupt solar increase retained stale history");
  require(block_transport_lighting_discontinuity(noon,sample(0,.5f)),"abrupt solar decrease retained stale history");
  require(block_transport_lighting_discontinuity(noon,sample(0,.75f,.3f)),"ambient intensity jump retained stale history");
  require(block_transport_lighting_discontinuity(noon,sample(0,.75f,.65f,.8f)),"sky visibility jump retained stale history");
  require(block_transport_lighting_discontinuity(noon,sample(0,.75f,.65f,1,.2f)),"twilight jump retained stale history");
  for(float energy:{1e-20f,.001f,.75f}) {
    const auto lit=sample(0,energy,energy),no_sun=sample(0,0,energy),dark=sample(0,0,0);
    require(block_transport_lighting_discontinuity(lit,no_sun),"sun shutoff retained even tiny old energy");
    require(block_transport_lighting_discontinuity(no_sun,dark),"environment shutoff retained even tiny old energy");
    require(block_transport_lighting_discontinuity(dark,lit),"source enable reused a zero-energy average");
    require(!block_transport_lighting_discontinuity(dark,dark),"steady darkness repeatedly reset history");
  }
  require(!block_transport_lighting_discontinuity(sample(0,0,0),sample(180,0,0,0,1)),
      "an entirely dark unused environment reset history");
}
void continuous_cases() {
  const auto first=sample(-90,.4f,.5f,.2f,.1f);auto submitted=first;
  for(unsigned frame=1;frame<=720;++frame) {
    const float fraction=float(frame)/720;
    const auto next=sample(-90+.25f*float(frame),.4f+.4f*fraction,.5f+.2f*fraction,.2f+.6f*fraction,.1f+.3f*fraction);
    require(!block_transport_lighting_discontinuity(submitted,next),
        "ordinary continuous evolution reset against an old fixed anchor");
    submitted=next;
  }
  require(block_transport_lighting_discontinuity(first,submitted),"accumulated half-day change fixture is too small");
  require(block_transport_lighting_discontinuity(submitted,sample(-90)),"clock discontinuity after a smooth sequence went undetected");
}
void finite_cases() {
  const auto noon=sample(0);
  const float huge=std::numeric_limits<float>::max();
  const auto normalized=block_transport_lighting({0,huge,0,.75f},{1,.65f,0,0});
  require(!block_transport_lighting_discontinuity(noon,normalized),"direction magnitude changed physical lighting");
  require(normalized.sun==noon.sun,"finite huge sun direction did not normalize safely");
  for(const Value bad:{Value{0,0,0,.75f},Value{1e-20f,0,0,.75f}}) {
    const auto safe=block_transport_lighting(bad,{1,.65f,0,0});
    require(safe.sun==Value{0,1,0,0},"degenerate direction did not disable the directional source");
    require(block_transport_lighting_discontinuity(noon,safe),"invalid directional source retained solar energy");
  }
  for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
      -std::numeric_limits<float>::infinity()}) {
    for(unsigned axis=0;axis<4;++axis) {
      auto sun=noon.sun;sun[axis]=invalid;
      const auto safe=block_transport_lighting(sun,noon.sky);
      for(float value:safe.sun)require(std::isfinite(value),"nonfinite sun uniform reached GPU policy output");
      require(safe.sun[3]==0 && block_transport_lighting_discontinuity(noon,safe),
          "invalid solar data retained direct history");
      const auto again=block_transport_lighting(sun,noon.sky);
      require(!block_transport_lighting_discontinuity(safe,again),"persistent invalid solar data repeatedly reset epochs");
    }
    for(unsigned component=0;component<3;++component) {
      auto sky=noon.sky;sky[component]=invalid;
      const auto safe=block_transport_lighting(noon.sun,sky);
      for(float value:safe.sky)require(std::isfinite(value),"nonfinite sky uniform reached GPU policy output");
      require(safe.sky[component]==0,"invalid sky component was not removed");
      if(component==1)require(block_transport_lighting_discontinuity(noon,safe),"invalid environment retained energy");
    }
  }
  const auto bounded=block_transport_lighting({0,1,0,-1},{-1,-1,2,42});
  require(bounded.sun[3]==0 && bounded.sky==Value{0,0,1,0},"environment domain canonicalization changed");
}
}
unsigned block_transport_lighting_cases() {
  transition_cases();continuous_cases();finite_cases();return checks;
}
