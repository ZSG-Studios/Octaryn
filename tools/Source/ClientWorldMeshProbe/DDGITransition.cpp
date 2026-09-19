#include "DDGITransitionFixture.h"
#include <algorithm>
#include <functional>

namespace mesh_probe {
void ddgi_transition_cases(mesh_probe::Fixture& f) {
  transition::Fixture probe(f);auto source=column();
  const auto stone=std::uint16_t(transition::material(f,"stone"));
  const auto torch=std::uint16_t(transition::material(f,"torch"));
  for(int z=0;z<32;++z)for(int x=0;x<32;++x)put(source,x,1,z,stone);
  for(int y=2;y<10;++y)for(int i=10;i<22;++i) {
    put(source,10,y,i,stone);put(source,21,y,i,stone);put(source,i,y,10,stone);put(source,i,y,21,stone);
  }
  probe.publish(source);
  std::vector<transition::Phase> phases;bool lit=false;double day=.5;
  const auto light=[&](bool enabled) {lit=enabled;put(source,16,2,16,enabled?torch:0);probe.publish(source);};
  const auto run=[&](std::string name,double duration,std::string kind,std::string from,std::string target,bool strict,
      const std::function<void(double)>& update={}) {
    transition::Phase phase;phase.name=std::move(name);phase.kind=std::move(kind);
    phase.from=std::move(from);phase.target=std::move(target);phase.strict=strict;
    const auto start=transition::Clock::now();const auto frames=probe.frames();double next=0;
    do {
      const double before=transition::seconds(start);if(update)update(std::min(before/duration,1.));
      probe.frame();const double elapsed=transition::seconds(start);
      if(elapsed<next)continue;next=elapsed+.1;
      phase.samples.push_back(probe.read(phase.name,elapsed,day,lit,frames));
    } while(transition::seconds(start)<duration);
    require(phase.samples.size()>=3,"DDGI transition insufficient real-time samples");
    std::printf("ddgi_transition_phase name=%s samples=%zu elapsed=%.3f energy=%.7f\n",phase.name.c_str(),phase.samples.size(),phase.samples.back().elapsed,transition::energy(phase.samples.back().receiver[0]));
    phases.push_back(std::move(phase));
  };
  run("ambient_sun_baseline",6,"steady","","",false);
  light(true);run("torch_added",6,"step","ambient_sun_baseline","torch_added",true);
  light(false);run("torch_removed",6,"step","torch_added","ambient_sun_baseline",true);
  for(unsigned cycle=0;cycle<8;++cycle) {
    light(true);run("rapid_add_"+std::to_string(cycle),.4,"rapid","ambient_sun_baseline","torch_added",true);
    light(false);run("rapid_remove_"+std::to_string(cycle),.4,"rapid","torch_added","ambient_sun_baseline",true);
  }
  run("rapid_recovery",6,"step","torch_added","ambient_sun_baseline",true);
  day=.4;probe.scene(day);run("sun_start",6,"step","ambient_sun_baseline","sun_start",false);
  day=.6;probe.scene(day);run("sun_end_reference",6,"step","sun_start","sun_end_reference",false);
  day=.4;probe.scene(day);run("sun_reset",6,"step","sun_end_reference","sun_start",true);
  run("moving_sun",12,"motion","sun_start","sun_end_reference",true,[&](double progress) {
    day=.4+.2*progress;probe.scene(day);
  });
  day=.6;probe.scene(day);run("moving_sun_hold",6,"step","sun_start","sun_end_reference",true);
  probe.check();const bool passed=transition::report(probe.directory,probe.backend,phases);
  require(passed,"DDGI transition strict stability/receiver gates failed; inspect JSON and CSV");
  std::puts("ddgi_transition=passed production_world_as=1 production_scheduler=1 production_receiver_sampling=1 gpu_controls=1 fine_radius=16 coarse_radius=128 retained_ambient=.65 retained_sun=.75 motion_day_start=.4 motion_day_end=.6 rapid_cycles=8");
}
}
