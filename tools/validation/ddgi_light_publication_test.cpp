#include "DDGISystem.h"
#include "LightingChanges.h"
#include <cstdio>
#include <stdexcept>

// CPU fixture for the production publication hook; renderer GPU owners omitted.
namespace octaryn::client::rendering {
struct WorldRenderer {
  DDGISystem ddgi;
  LightingChanges lighting_changes;
  struct {std::vector<WorldLocalLight> lights;std::uint64_t light_revision{};} local_lighting;
  SkyUniforms sky{};
  SkyLighting lighting{};
};
}
#include "DDGILightingChangesUnderTest.h"
using namespace octaryn::client::rendering;
namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void mature(DDGISystem& s) {
  s.available=true;s.config.counts={2,2,2};s.config.spacing=1;s.config.budget=8;
  s.dispatch_capacity=8;s.control_data.resize(8);s.last_updates.assign(8,100);
  s.last_update_times.assign(8,1);s.dirty.assign(8,false);s.occupancy.assign(8,0);
  ddgi_scroll(s,{0,0,0});
  s.last_updates.assign(8,100);s.dirty.assign(8,false);s.frame=100;s.time_seconds=1;
}
void tick(DDGISystem& s) {
  ++s.frame;s.time_seconds+=1./60;ddgi_schedule(s,{0,0,0});
}
void source_additions() {
  for(unsigned type:{0u,1u,2u,3u}) {
    WorldRenderer r;mature(r.ddgi);r.ddgi.fine_volume=std::make_unique<DDGISystem>();
    mature(*r.ddgi.fine_volume);
    world_ddgi_lighting_changes(r);
    WorldLocalLight light;light.position_range={0,0,0,8};light.axis_v_type[3]=float(type);
    r.local_lighting.lights={light};++r.local_lighting.light_revision;
    world_ddgi_lighting_changes(r);
    for(auto* s:{&r.ddgi,r.ddgi.fine_volume.get()}) {
      for(unsigned i=0;i<8;++i) {
        require(s->dirty[i] && s->control_data[i].refresh_frame>s->last_updates[i],
          "new source retained mature irradiance history");
        require(s->control_data[i].padding[2]==(DDGIGentleWake|DDGILightingOnly),"new source excluded cage samples or requested geometry reset");
      }
      tick(*s);
      require(s->selected.size()==8,"new source was not scheduled immediately");
      for(auto packed:s->selected)require((packed>>30)==1,"gentle source update expanded the lighting ray count");
    }
    const auto refresh=r.ddgi.control_data[0].refresh_frame;
    // Both unchanged API revisions and ordinary frames must leave history alone.
    for(unsigned frame=0;frame<120;++frame) {
      ++r.local_lighting.light_revision;world_ddgi_lighting_changes(r);
      require(r.ddgi.control_data[0].refresh_frame==refresh,"unchanged source repeatedly snapped history");
      require(std::none_of(r.ddgi.dirty.begin(),r.ddgi.dirty.end(),[](bool v){return v;}),
        "unchanged source repeatedly dirtied probes");
      tick(r.ddgi);tick(*r.ddgi.fine_volume);
    }
    r.local_lighting.lights.clear();++r.local_lighting.light_revision;world_ddgi_lighting_changes(r);
    for(unsigned i=0;i<8;++i)require(r.ddgi.control_data[i].padding[2]==(DDGIHardReject|DDGILightingOnly) &&
      r.ddgi.control_data[i].refresh_frame>r.ddgi.last_updates[i],"removal lost hard rejection");
  }
}
void removal_overlap() {
  DDGISystem s;mature(s);
  const std::array<float,3> low{-8,-8,-8},high{8,8,8};
  ddgi_invalidate(s,low,high,0,true,true);
  ddgi_invalidate(s,low,high,0,true,false,true);
  require((s.control_data[0].padding[2]&DDGIHardReject)!=0,"addition retired an untraced removal");
  tick(s);
  // A real addition after a completed removal needs fresh history, not exclusion.
  ddgi_invalidate(s,low,high,0,true,false,true);
  require(s.control_data[0].padding[2]==(DDGIGentleWake|DDGILightingOnly),"addition extended completed removal exclusion");
  require(s.control_data[0].refresh_frame>s.last_updates[0],"overlapping addition failed to refresh");
  tick(s);
  const auto refresh=s.control_data[0].refresh_frame;
  for(unsigned frame=0;frame<60;++frame) {
    ddgi_invalidate(s,low,high,0,true,false);
    require(s.control_data[0].refresh_frame==refresh,"gentle wake extended history refresh");
    tick(s);
  }
  ddgi_invalidate(s,low,high,0,true,true);tick(s);
  const auto removal=s.control_data[0].refresh_frame;
  ddgi_invalidate(s,low,high,0,true,false);tick(s);
  require(!(s.control_data[0].padding[2]&DDGIHardReject) && s.control_data[0].refresh_frame==removal,
    "gentle wake pinned completed removal generation");
}
void geometry_precedence() {
  for(bool geometryFirst:{false,true}) {
    DDGISystem s;mature(s);
    const std::array<float,3> low{-8,-8,-8},high{8,8,8};
    if(geometryFirst)ddgi_invalidate(s,low,high,0);
    ddgi_invalidate(s,low,high,0,true,true);
    if(!geometryFirst)ddgi_invalidate(s,low,high,0);
    require(!(s.control_data[0].padding[2]&DDGILightingOnly),"lighting event concealed pending geometry recheck");
    require((s.control_data[0].padding[2]&DDGIHardReject)!=0,"geometry event lost pending removal rejection");
    tick(s);
    for(auto packed:s.selected)require((packed>>30)==0,"pending geometry lost full ray tier");
  }
}
void environment_publication() {
  WorldRenderer r;mature(r.ddgi);world_ddgi_lighting_changes(r);
  // Sky-visible probes request the bounded response; interior probes never
  // enter the noisy reactive window from sun motion alone.
  for(unsigned i=0;i<r.ddgi.control_data.size();++i)r.ddgi.occupancy[i]=i%2?2u:0u;
  const auto interior=r.ddgi.control_data[0].refresh_frame;
  const auto controls=r.ddgi.control_data;
  // A large radiometric jump is classified discontinuous and requests a bounded
  // global response without touching probe identity, geometry or version.
  r.lighting.sun_strength=1;r.sky.light_direction_sky[0]=0;r.sky.light_direction_sky[1]=-1;
  r.sky.light_direction_sky[2]=0;r.sky.light_direction_sky[3]=0;
  world_ddgi_lighting_changes(r);
  require(r.ddgi.control_data[1].refresh_frame>interior,"abrupt environment change did not refresh sky probes");
  require(r.ddgi.burst_frames==0,"abrupt environment change started a global burst stampede");
  require(r.ddgi.control_data[1].padding[2]==(DDGIGentleWake|DDGILightingOnly),
    "environment response altered geometry or excluded samples");
  require(r.ddgi.control_data[0].refresh_frame==interior&&!r.ddgi.response_updates[0],
    "interior probe entered the reactive window from sun motion");
  for(unsigned i=0;i<controls.size();++i)require(r.ddgi.control_data[i].version==controls[i].version,
    "environment response changed probe identity/version");
  // Gradual drift must stay on the ordinary cadence and never republish controls.
  DDGISystem quiet;mature(quiet);
  const auto quietRefresh=quiet.control_data[0].refresh_frame;
  const bool quietDirty=quiet.controls_dirty;const unsigned quietBurst=quiet.burst_frames;
  ddgi_environment_changed(quiet,false);
  require(quiet.controls_dirty==quietDirty&&quiet.burst_frames==quietBurst &&
    quiet.control_data[0].refresh_frame==quietRefresh,"gradual drift requested global work");
}
}
int main() {
  try {source_additions();removal_overlap();geometry_precedence();environment_publication();}
  catch(const std::exception& e) {std::fprintf(stderr,"ddgi_light_publication_test=failed reason=%s\n",e.what());return 1;}
  std::puts("ddgi_light_publication_test=passed source_types=4 volumes=2 lighting_only=1 unchanged_no_snap=1 removal_lifetime=1 geometry_precedence=1 environment_no_global_reset=1");
}
