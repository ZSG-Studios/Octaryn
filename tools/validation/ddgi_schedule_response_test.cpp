#include "DDGISystem.h"
#include "DDGIVolumeConfig.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string_view>
using namespace octaryn::client::rendering;
namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
unsigned index(unsigned packed) {return packed&0x3fffffffu;}
DDGISystem volume(unsigned radius,bool fine) {
  DDGISystem s;s.config=ddgi_volume_config(DDGIConfig{},radius,fine,8);s.config.rays=176;
  s.cell_centered=fine;s.dispatch_capacity=4096;
  const unsigned count=radius?s.config.counts[0]*s.config.counts[1]*s.config.counts[2]:0;
  s.control_data.resize(count);s.last_updates.resize(count);s.last_update_times.resize(count);
  s.dirty.resize(count,true);s.occupancy.resize(count);
  if(count)ddgi_scroll(s,{0,0,0});
  return s;
}
void tick(DDGISystem& s,unsigned fps) {
  ++s.frame;s.frame_seconds=1./fps;s.time_seconds+=s.frame_seconds;
  if(!s.control_data.empty())ddgi_schedule(s,{0,0,0});
}
void measured(DDGISystem& s,double probesPerFrame) {
  ddgi_budget_sample(s,.35,static_cast<unsigned>(probesPerFrame*(s.config.rays+64)));
}
void fairness(unsigned fps,bool priorityOnly=false) {
  auto s=volume(2,true);s.config.budget=1;s.dispatch_capacity=1;
  std::fill(s.last_updates.begin(),s.last_updates.end(),1);s.frame=1;
  // One fresh probe must compete with repeated edits and stale clean history.
  s.last_updates.back()=0;std::fill(s.dirty.begin(),s.dirty.end(),false);
  unsigned freshFrame=0,stableFrame=0;
  for(unsigned frame=0;frame<fps*6;++frame) {
    s.dirty[0]=true;
    if(priorityOnly)s.scheduled_work=0;
    tick(s,fps);
    require(s.selected.size()<=1,"low cap exceeded");
    for(auto packed:s.selected) {
      if(index(packed)==s.control_data.size()-1 && !freshFrame)freshFrame=frame+1;
      if(index(packed)==1 && !stableFrame)stableFrame=frame+1;
    }
  }
  require(freshFrame && double(freshFrame)/fps<.2,"fractional-budget alias starved fresh probe");
  require(stableFrame && double(stableFrame)/fps<5,"continuous dirty light starved clean probe");
  std::printf("fairness fps=%u fresh_seconds=%.4f clean_seconds=%.4f cap=1\n",fps,double(freshFrame)/fps,double(stableFrame)/fps);
}
void hard_retirement(unsigned fps) {
  auto s=volume(1,true);s.dispatch_capacity=1;s.config.budget=3;
  std::fill(s.last_updates.begin(),s.last_updates.end(),1);std::fill(s.dirty.begin(),s.dirty.end(),false);
  s.frame=1;
  ddgi_invalidate(s,{-100,-100,-100},{100,100,100},0,true,true);
  for(unsigned frame=0;frame<fps;++frame) {
    // A gentle wake must not pin an already-consumed removal generation.
    ddgi_invalidate(s,{-100,-100,-100},{100,100,100},0,true,false);
    tick(s,fps);
    for(unsigned i=0;i<s.control_data.size();++i) {
      if(s.last_updates[i]<s.control_data[i].refresh_frame)
        require((s.control_data[i].padding[2]&DDGIHardReject)!=0,"untraced removal marker retired");
      else if(s.last_updates[i]<s.frame)
        require((s.control_data[i].padding[2]&DDGIHardReject)==0,"gentle wake pinned completed hard generation");
    }
  }
  require(std::none_of(s.last_updates.begin(),s.last_updates.end(),[](auto frame){return frame==1;}),
    "repeated revisions starved pending removal");
  ddgi_invalidate(s,{-100,-100,-100},{100,100,100},0,true,true);
  for(unsigned i=0;i<s.control_data.size();++i)
    require(s.control_data[i].refresh_frame>s.last_updates[i],"new removal reused consumed generation");
}
void streaming(unsigned fps) {
  auto s=volume(3,true);s.config.budget=8;s.dispatch_capacity=8;
  std::fill(s.last_updates.begin(),s.last_updates.end(),1);std::fill(s.dirty.begin(),s.dirty.end(),false);s.frame=1;
  const unsigned pending=0;s.control_data[pending].padding[1]=1;
  ddgi_invalidate(s,{-100,-100,-100},{100,100,100},0,true,true);
  for(unsigned frame=0;frame<fps*2;++frame) {
    if(frame==fps/2)s.control_data[pending].padding[1]=0;
    // Simulate fresh publication without changing the retained logical cell.
    const unsigned arriving=1+frame%(s.control_data.size()-1);
    s.last_updates[arriving]=0;s.last_update_times[arriving]=s.time_seconds;s.dirty[arriving]=true;
    tick(s,fps);
    if(frame<fps/2)require(s.last_updates[pending]==1 && s.dirty[pending],"seed-only wake was consumed without trace");
  }
  require(s.last_updates[pending]>1 && !(s.control_data[pending].padding[2]&DDGIHardReject),
    "pending hard removal was lost or starved during streaming");
}
void matrix(unsigned fine,unsigned coarse,unsigned fps,bool fixed) {
  auto f=volume(fine,true),c=volume(coarse,false);
  for(auto* s:{&f,&c}) {
    for(unsigned sample=0;sample<40 && !fixed;++sample)measured(*s,1024);
    // Controlled 0.35 ms per 1024 full-ray probes, not a GPU performance claim.
    const unsigned count=unsigned(s->control_data.size());
    const double deadline=count/(1024.*60)+.35;
    unsigned frames=0;
    for(;frames<unsigned(std::ceil(deadline*fps));++frames) {
      tick(*s,fps);
      require(s->selected.size()<=s->dispatch_capacity,"large volume exceeded allocation cap");
      if(std::none_of(s->last_updates.begin(),s->last_updates.end(),[](auto frame){return frame==0;}))break;
    }
    require(std::none_of(s->last_updates.begin(),s->last_updates.end(),[](auto frame){return frame==0;}),
      "volume-sized backlog missed measured-throughput sweep bound");
    std::printf("backlog fine=%u coarse=%u volume=%s fps=%u probes=%u sweep_seconds=%.4f deadline=%.4f\n",
      fine,coarse,s==&f?"fine":"coarse",fps,count,double(frames+1)/fps,deadline);
    // Reproduce retained tunnel histories competing with continuous publications.
    // The budget remains measured and bounded; 1/4 of work must service age.
    if(count && count<=32768) {
      const auto before=s->last_updates;
      const double rate=std::min(s->adaptive_budget,std::max(double(s->config.budget),count/15.));
      const double fairDeadline=4*count/(rate*60)+.3;
      for(unsigned frame=0;frame<unsigned(std::ceil(fairDeadline*fps));++frame) {
        for(unsigned i=0;i<count/2;++i)s->dirty[i]=true;
        tick(*s,fps);
      }
      for(unsigned i=0;i<count;++i)
        require(s->last_updates[i]>before[i],"dirty publication backlog starved retained tunnel history");
      std::printf("retained_history fps=%u probes=%u all_refreshed=1 bound_seconds=%.4f oldest_seconds=%.4f\n",
        fps,count,fairDeadline,s->stats.oldest_update_seconds);
    }
  }
}
void controller() {
  auto s=volume(16,true);
  for(unsigned i=0;i<40;++i)measured(s,1024);
  require(s.adaptive_budget>1000,"available measured headroom was never used");
  measured(s,16);
  require(s.adaptive_budget<=16,"expensive GPU measurement did not immediately reduce budget");
  const double reduced=s.adaptive_budget;
  ddgi_budget_sample(s,0,100);ddgi_budget_sample(s,NAN,100);
  require(s.adaptive_budget==reduced,"invalid timing changed budget");
  measured(s,1024);
  require(s.adaptive_budget<=reduced*1.125+1e-6,"budget recovered in an unbounded burst");
  s.frame_seconds=10;s.frame=1;s.time_seconds=10;ddgi_schedule(s,{0,0,0});
  require(s.selected.size()<=unsigned(s.adaptive_budget*6)+1,"long pause banked unbounded work");
  std::fill(s.dirty.begin(),s.dirty.end(),false);s.occupancy.assign(s.control_data.size(),1);
  tick(s,144);require(s.selected.empty() && s.budget_credit==0,"idle volume banked later burst credit");
}
void dispatch_overhead(unsigned fps,bool noBatch) {
  auto s=volume(16,true);s.frame=1;
  std::fill(s.last_updates.begin(),s.last_updates.end(),1);
  std::fill(s.dirty.begin(),s.dirty.end(),false);
  unsigned work=0,dispatches=0;
  // Closed-loop: fixed command/trace/update overhead plus marginal ray work.
  // This reproduces the 2--5-probe budget collapse observed in the uncapped run.
  for(unsigned frame=0;frame<fps*5;++frame) {
    const double budget=s.adaptive_budget;
    if(noBatch && budget>0) {s.config.budget=std::max(1u,unsigned(budget));s.adaptive_budget=0;}
    tick(s,fps);
    if(noBatch)s.adaptive_budget=budget;
    if(s.selected.empty())continue;
    const unsigned units=s.stats.scheduled_rays+64*s.stats.updated_probes;
    ddgi_budget_sample(s,.045+units*.000001,units,s.stats.updated_probes,s.selection_target);
    work+=s.stats.updated_probes;++dispatches;
  }
  require(s.adaptive_budget>900,"fixed dispatch overhead collapsed adaptive throughput");
  require(work>200000,"uncapped controller left most retained histories stale");
  require(s.stats.oldest_update_seconds<.8,"uncapped dispatch overhead starved retained history");
  std::printf("overhead fps=%u budget_per_60=%.2f probes=%u dispatches=%u oldest_seconds=%.4f\n",
    fps,s.adaptive_budget,work,dispatches,s.stats.oldest_update_seconds);
}
void tail_measurement(bool negative) {
  auto s=volume(16,true);s.adaptive_budget=494;
  s.milliseconds_per_work=.35/(494*240.);
  // Actual diagnostic frame 3296: 19 probes, trace .05656 + update .01788 ms.
  ddgi_budget_sample(s,.07444,2736,negative?0:19,411);
  require(s.adaptive_budget==494,"19-probe diagnostic tail poisoned full-batch estimate");
  require(s.gpu_debt_seconds>0 && s.gpu_debt_seconds<.004,"tail GPU overhead was not charged");
  const double debt=s.gpu_debt_seconds;
  s.budget_credit=100;
  ddgi_budget_sample(s,.07444,2736,19,411);
  require(s.budget_credit<20 && s.gpu_debt_seconds==debt,"tail did not charge banked credit first");
  ddgi_budget_sample(s,4,59184,411,411);
  require(s.adaptive_budget<30,"genuinely expensive full batch did not back off");
  const double beforeDebt=s.gpu_debt_seconds,beforeCredit=s.budget_credit;
  const double probeCost=s.milliseconds_per_work*240;
  ddgi_budget_sample(s,2,2736,19,411);
  const double charged=(s.gpu_debt_seconds-beforeDebt)*21+(beforeCredit-s.budget_credit)*probeCost;
  require(std::abs(charged-(2-19*probeCost))<1e-9,"expensive partial dispatch escaped GPU allowance");
}
void partial_publications(unsigned fps) {
  auto s=volume(16,true);s.frame=1;s.config.budget=512;
  s.adaptive_budget=494;s.milliseconds_per_work=.35/(494*240.);
  std::fill(s.last_updates.begin(),s.last_updates.end(),1);
  s.occupancy.assign(s.control_data.size(),1);
  constexpr unsigned arrivals[]{1,19,76,101,411,1024};
  double gpu=0;unsigned partial=0,full=0;
  for(unsigned frame=0;frame<fps*5;++frame) {
    const unsigned active=arrivals[(frame/std::max(1u,fps/5))%6];
    for(unsigned i=0;i<1024;++i) {s.occupancy[i]=i<active?0:1;s.dirty[i]=i<active;}
    tick(s,fps);
    if(s.selected.empty())continue;
    const unsigned work=s.stats.scheduled_rays+64*s.stats.updated_probes;
    const double milliseconds=.045+work*.000001;gpu+=milliseconds;
    if(s.stats.updated_probes<s.selection_target)++partial;else ++full;
    ddgi_budget_sample(s,milliseconds,work,s.stats.updated_probes,s.selection_target);
  }
  require(partial && full,"partial publication fixture missed partial or full dispatches");
  require(s.adaptive_budget>=494,"partial publication workload collapsed throughput estimate");
  require(gpu<5*.35*60+.7,"partial fixed overhead exceeded earned GPU allowance");
  std::printf("partial_publications fps=%u partial=%u full=%u gpu_ms=%.3f allowance_ms=105 budget=%.2f debt_seconds=%.5f\n",
    fps,partial,full,gpu,s.adaptive_budget,s.gpu_debt_seconds);
}
void environment_cadence(unsigned fps,bool negative=false) {
  auto s=volume(1,true);s.config.budget=8;s.dispatch_capacity=8;s.frame=1;
  std::fill(s.last_updates.begin(),s.last_updates.end(),1);
  std::fill(s.dirty.begin(),s.dirty.end(),false);
  for(unsigned i=0;i<8;++i)s.occupancy[i]=i<4?0:2;
  s.controls_dirty=false;
  unsigned observations=0;
  for(unsigned frame=0;frame<fps*2;++frame) {
    // Gradual drift relies on the ordinary age-based cadence: no global control
    // republish, no urgent burst, and no per-frame ray-noise injection. The
    // negative control requests abrupt global responses instead and must trip it.
    if(frame%std::max(1u,fps/4)==0)ddgi_environment_changed(s,negative);
    tick(s,fps);
    observations+=unsigned(s.selected.size());
    require(!s.controls_dirty,"gradual sun tracking republished global controls");
    require(!s.burst_frames,"gradual sun tracking restarted a dirty burst");
  }
  require(observations>=8,"gradual sun tracking stopped ordinary observation cadence");
  std::printf("environment_cadence fps=%u observations=%u seconds=2 global_control_uploads=0\n",
    fps,observations);
}
void environment_abrupt_response(unsigned fps) {
  auto s=volume(1,true);s.config.budget=8;s.dispatch_capacity=8;s.frame=1;
  std::fill(s.last_updates.begin(),s.last_updates.end(),1);
  std::fill(s.dirty.begin(),s.dirty.end(),false);
  for(auto& probe:s.control_data)probe.padding[1]=0;
  // Half the field sees sky; interior probes must keep the mature filter.
  for(unsigned i=0;i<8;++i)s.occupancy[i]=i<4?0u:2u;
  s.controls_dirty=false;
  ddgi_environment_changed(s,true);
  require(s.controls_dirty,"abrupt environment change was ignored");
  for(unsigned i=0;i<8;++i) {
    if(i>=4)require(s.response_updates[i]!=0&&s.control_data[i].refresh_frame>=1u,
      "sky-visible probe did not request a bounded response");
    else require(s.response_updates[i]==0&&s.control_data[i].refresh_frame==0u,
      "interior probe entered the noisy reactive window from sun motion");
  }
  require(s.burst_frames==0,"abrupt environment change started a global burst stampede");
  unsigned frames=0;
  while(frames<fps*2 && std::any_of(s.response_updates.begin(),s.response_updates.end(),[](auto n){return n!=0;})) {
    tick(s,fps);++frames;
  }
  require(double(frames)/fps<.6,"abrupt environment response retained slow age cadence");
  std::printf("environment_abrupt fps=%u observations_seconds=%.4f\n",fps,double(frames)/fps);
}
void local_response(unsigned fps) {
  for(bool removal:{false,true}) {
    auto s=volume(1,true);s.config.budget=8;s.dispatch_capacity=8;s.frame=1;
    std::fill(s.last_updates.begin(),s.last_updates.end(),1);
    std::fill(s.dirty.begin(),s.dirty.end(),false);
    ddgi_invalidate(s,{-8,-8,-8},{8,8,8},0,true,removal,true);
    const auto generation=s.control_data[0].refresh_frame;
    unsigned frames=0;
    while(frames<fps && std::any_of(s.response_updates.begin(),s.response_updates.end(),[](auto n){return n!=0;})) {
      tick(s,fps);++frames;
      require(s.control_data[0].refresh_frame==generation,"response followups repeatedly reset history generation");
    }
    require(double(frames)/fps<.5,"bounded local response retained slow age cadence");
    require(std::all_of(s.response_updates.begin(),s.response_updates.end(),[](auto n){return n==0;}),
      "local source event never finished bounded observation response");
    tick(s,fps);
    require(s.selected.empty(),"completed local event kept urgent dispatch active");
    std::printf("local_response fps=%u removal=%u four_observations_seconds=%.4f\n",fps,unsigned(removal),double(frames)/fps);
  }
}
void environment_dirty_fairness(unsigned fps) {
  auto s=volume(8,true);s.config.budget=128;s.dispatch_capacity=128;s.frame=1;
  std::fill(s.last_updates.begin(),s.last_updates.end(),1);
  std::fill(s.dirty.begin(),s.dirty.end(),false);
  const unsigned count=unsigned(s.control_data.size());
  // Local edits occupy one half; sky-visible probes occupy the other.
  for(unsigned i=0;i<count;++i)s.occupancy[i]=i<count/2?0u:2u;
  const double deadline=4*count/(128.*60)+.3;
  unsigned work=0;
  for(unsigned frame=0;frame<unsigned(std::ceil(deadline*fps));++frame) {
    ddgi_environment_changed(s,true);
    for(unsigned i=0;i<count/2;++i)s.dirty[i]=true;
    tick(s,fps);work+=unsigned(s.selected.size());
    require(s.selected.size()<=128,"environment with edits exceeded dispatch cap");
  }
  require(std::all_of(s.last_updates.begin(),s.last_updates.end(),[](auto n){return n>1;}),
    "continuous local edits starved sun tracking on retained probes");
  require(work<=unsigned(std::ceil(deadline*fps))*128.*60/fps+1,"dirty sun workload exceeded work credit");
  std::printf("environment_dirty_fairness fps=%u probes=%u bound_seconds=%.3f work=%u\n",fps,count,deadline,work);
}
}
int main(int argc,char** argv) {
  try {
    const bool fixed=argc>1 && std::string_view(argv[1])=="--fixed-budget-negative-control";
    if(fixed) {matrix(16,128,60,true);return 0;}
    if(argc>1 && std::string_view(argv[1])=="--priority-only-negative-control") {fairness(240,true);return 0;}
    if(argc>1 && std::string_view(argv[1])=="--unbatched-negative-control") {dispatch_overhead(600,true);return 0;}
    if(argc>1 && std::string_view(argv[1])=="--tail-negative-control") {tail_measurement(true);return 0;}
    if(argc>1 && std::string_view(argv[1])=="--environment-age-negative-control") {environment_cadence(60,true);return 0;}
    controller();
    tail_measurement(false);
    for(unsigned fps:{30u,60u,144u,600u,2000u})partial_publications(fps);
    for(unsigned fps:{30u,60u,144u,240u,600u,2000u})dispatch_overhead(fps,false);
    fairness(240);
    for(unsigned fps:{30u,60u,144u}) {
      environment_cadence(fps);environment_abrupt_response(fps);local_response(fps);environment_dirty_fairness(fps);
      fairness(fps);hard_retirement(fps);streaming(fps);
      for(auto radii:{std::array<unsigned,2>{0,0},{0,128},{6,0},{16,128},{32,1024}})
        matrix(radii[0],radii[1],fps,false);
    }
    std::puts("ddgi_schedule_response_test=passed");return 0;
  } catch(const std::exception& e) {
    std::fprintf(stderr,"ddgi_schedule_response_test=failed reason=%s\n",e.what());return 1;
  }
}
