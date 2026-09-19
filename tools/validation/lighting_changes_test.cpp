#include "LightingChanges.h"
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering;
namespace {
unsigned checks{};
void require(bool ok,const char* reason) {
  ++checks;
  if(!ok)throw std::runtime_error(reason);
}
struct Wake {LightInfluence bounds;bool hard;};
std::vector<Wake> publish(LocalLightChanges& changes,const std::vector<WorldLocalLight>& lights) {
  std::vector<Wake> result;
  changes.update(lights,[&](const LightInfluence& bounds,bool hard){result.push_back({bounds,hard});});
  return result;
}
void local_changes() {
  WorldLocalLight light;light.position_range={-10,5,-20,8};
  WorldLocalLight other=light;other.position_range[0]=1000;
  LocalLightChanges changes;
  auto result=publish(changes,{light,other});
  require(result.size()==2 && !result[0].hard && !result[1].hard,"additions were not gentle");
  require(publish(changes,{other,light}).empty(),"reordering woke unchanged lights");
  const auto initial=light;
  const auto changed=[&](const char* reason) {
    const auto wakes=publish(changes,{light,other});
    require(wakes.size()==2,reason);
    require(wakes[0].hard!=wakes[1].hard,"changed source lost hard old influence");
    for(const auto& wake:wakes)require(wake.bounds.maximum[0]<100,"unrelated light invalidated");
    require(publish(changes,{light,other}).empty(),"stable light kept waking");
  };
  light.position_range[0]+=.1f;changed("sub-half-voxel motion was ignored");
  light.position_range[3]=2;changed("range contraction was ignored");
  light.color_intensity[0]=.2f;changed("color change was ignored");
  light.color_intensity[3]=0;changed("intensity removal was ignored");
  light.axis_v_type[3]=1;changed("point to spot change was ignored");
  light.direction_outer[0]=.2f;changed("spot direction was ignored");
  light.direction_outer[3]=.3f;changed("outer cone change was ignored");
  light.axis_u_inner[3]=.8f;changed("inner cone change was ignored");
  light.axis_v_type[3]=2;light.axis_u_inner={20,1,0,.8f};light.axis_v_type={-3,0,4,2};
  changed("rectangle conversion was ignored");
  auto bounds=light_influence(light);
  require(bounds.minimum[0]==light.position_range[0]-25 && bounds.maximum[2]==-14,
    "rectangle influence did not include both half axes plus range");
  light.axis_u_inner[0]=30;changed("rectangle resize was ignored");
  result=publish(changes,{other});
  require(result.size()==1 && result[0].hard,"removal did not reject full old influence");
  require(result[0].bounds.minimum[0]==light.position_range[0]-35,"removed rectangle lost old extent");
  LocalLightChanges colocated;
  auto colored=initial;colored.color_intensity[1]=0;
  publish(colocated,{initial,colored,initial});
  require(publish(colocated,{initial,initial,colored}).empty(),"colocated reorder changed multiplicity");
  result=publish(colocated,{initial,colored});
  require(result.size()==1 && result[0].hard,"colocated duplicate removal was missed");
  LocalLightChanges movement;
  publish(movement,{initial});light=initial;light.position_range[0]=200;
  result=publish(movement,{light});
  require(result.size()==2 && result[0].hard && !result[1].hard,"move lost old/new union");
  require(result[0].bounds.minimum[0]==-18 && result[1].bounds.maximum[0]==208,
    "movement bounds were clipped to new source");
}
void environment_changes() {
  lighting_settings settings{};settings.sun_strength=.75f;settings.ambient_strength=.65f;settings.skylight_floor=.25f;
  auto sky=make_sky_uniforms(.5,0,{});
  auto light=make_sky_lighting(.5,settings);
  EnvironmentChanges changes;
  require(changes.update(sky,light,0)==EnvironmentChange::None,"initialization reset history");
  // Actual renderer sky builder changes animation time every frame, independent
  // of radiometric day time. This must not perpetually reset a static scene.
  for(unsigned frame=1;frame<=144;++frame)
    require(changes.update(make_sky_uniforms(.5,frame/144.,{}),light,frame/144.)==EnvironmentChange::None,
      "frame time masqueraded as environment change");
  settings.sun_strength=0;light=make_sky_lighting(.5,settings);
  require(changes.update(sky,light,1.001)==EnvironmentChange::Discontinuous,"sun off did not wake immediately");
  settings.ambient_strength=.2f;light=make_sky_lighting(.5,settings);
  require(changes.update(sky,light,1.002)==EnvironmentChange::Discontinuous,"ambient slider did not wake immediately");
  settings.skylight_floor=.05f;light=make_sky_lighting(.5,settings);
  require(changes.update(sky,light,1.003)==EnvironmentChange::Discontinuous,"sky floor did not wake");
  sky=make_sky_uniforms(.25,0,{});light=make_sky_lighting(.25,settings);
  require(changes.update(sky,light,1.004)==EnvironmentChange::Discontinuous,"day/night/twilight change was missed");
  sky=make_sky_uniforms(.2501,0,{});light=make_sky_lighting(.2501,settings);
  require(changes.update(sky,light,1.005)==EnvironmentChange::None,"smooth day motion woke every frame");
  require(changes.update(sky,light,1.3)==EnvironmentChange::Gradual,"throttled change was acknowledged without waking");
  require(changes.update(sky,light,2)==EnvironmentChange::None,"consumed environment kept waking");
  auto toggles=SkySettings{};toggles.stars=false;toggles.sun=false;toggles.moon=false;
  require(changes.update(make_sky_uniforms(.2501,100,toggles),light,3)==EnvironmentChange::None,
    "visual celestial toggles invalidated diffuse light");
  auto tiny=sky;tiny.light_direction_sky[0]+=1e-6f;
  require(changes.update(tiny,light,4)==EnvironmentChange::None,"floating point jitter caused reset");

  EnvironmentChanges night;
  const auto dark=make_sky_lighting(.85,settings);
  night.update(make_sky_uniforms(.85,0,{}),dark,0);
  for(unsigned frame=1;frame<=600;++frame) {
    const double fraction=.85+frame/(1200.*60);
    require(night.update(make_sky_uniforms(fraction,frame/60.,{}),dark,frame/60.)==EnvironmentChange::None,
      "zero-energy sun below horizon generated directional notifications");
  }
  // Solar direction still changes sky tint near dawn even with direct sun off.
  auto dawn=make_sky_uniforms(.25,0,{});
  require(night.update(dawn,dark,11)!=EnvironmentChange::None,
    "unlit sun elevation lost its real diffuse sky tint transition");
  settings.sun_strength=.75f;settings.ambient_strength=.65f;
  EnvironmentChanges natural;
  natural.update(make_sky_uniforms(.4,0,{}),make_sky_lighting(.4,settings),0);
  unsigned gradual=0;
  for(unsigned frame=1;frame<=600;++frame) {
    const double fraction=.4+frame/(1200.*60);
    const auto change=natural.update(make_sky_uniforms(fraction,frame/60.,{}),
      make_sky_lighting(fraction,settings),frame/60.);
    require(change!=EnvironmentChange::Discontinuous,"natural orbit became global reset");
    gradual+=change==EnvironmentChange::Gradual;
  }
  require(gradual>0 && gradual<=40,"natural progression did not remain bounded gradual work");
  settings.sun_strength=0;
  require(natural.update(make_sky_uniforms(.41,10.001,{}),make_sky_lighting(.41,settings),10.001)==
    EnvironmentChange::Discontinuous,"rapid sun removal was lost after natural progression");
}
}
int main() {
  try {local_changes();environment_changes();}
  catch(const std::exception& error) {std::fprintf(stderr,"lighting_changes_test=failed reason=%s\n",error.what());return 1;}
  std::printf("lighting_changes_test=passed checks=%u\n",checks);
}
