#include "SkyData.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace octaryn::client::rendering {
namespace {
float smooth(float lo,float hi,float x) {
    const float t=std::clamp((x-lo)/(hi-lo),0.0f,1.0f);
    return t*t*(3-2*t);
}
struct TimeVisuals { float orbit,elevation,daylight,sunlight,twilight,sky; };
TimeVisuals visuals(double fraction) {
    if(!std::isfinite(fraction)) fraction=0.5;
    fraction-=std::floor(fraction);
    TimeVisuals t{};
    t.orbit=static_cast<float>(fraction)*2*std::numbers::pi_v<float>-std::numbers::pi_v<float>/2;
    t.elevation=std::sin(t.orbit);
    t.daylight=smooth(-0.18f,0.10f,t.elevation);
    t.sunlight=smooth(-0.02f,0.18f,t.elevation);
    t.twilight=smooth(-0.28f,0.08f,t.elevation)*(1-smooth(0.08f,0.32f,t.elevation));
    t.sky=std::clamp(t.daylight+t.twilight*0.28f,0.06f,1.0f);
    return t;
}
}
SkyUniforms make_sky_uniforms(double day_fraction,double animation_seconds,const SkySettings& settings) {
    const auto t=visuals(day_fraction);
    const float pitch=-t.orbit,yaw=-std::numbers::pi_v<float>/2;
    const float time=std::isfinite(animation_seconds) ? static_cast<float>(std::fmod(animation_seconds,65536.0)) : 0;
    return {{std::cos(pitch)*std::sin(yaw),std::sin(pitch),-std::cos(pitch)*std::cos(yaw),t.sky},
            {t.twilight,smooth(-0.28f,-0.08f,t.elevation),settings.gradient?1.0f:0.0f,time},
            {settings.stars?1.0f:0.0f,settings.sun?1.0f:0.0f,settings.moon?1.0f:0.0f,0}};
}
SkyLighting make_sky_lighting(double day_fraction,const lighting_settings& settings) {
    const auto t=visuals(day_fraction);
    const float gameplay=std::max(t.sky,0.42f);
    const float ambient_scale=0.18f+std::pow(gameplay,0.95f)*0.82f;
    return {t.sky,gameplay,std::clamp(0.04f+settings.skylight_floor*std::pow(t.sky,1.02f),0.03f,settings.skylight_floor),
            settings.ambient_strength*(0.28f+ambient_scale*0.72f),
            settings.sun_strength*t.sunlight,settings.sun_fallback_strength*t.sunlight};
}
}
