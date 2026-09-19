#include "DDGITransitionFixture.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>

namespace mesh_probe::transition {
namespace {
constexpr double TailSeconds=.6,SettleWindow=.5;
const Phase& find(const std::vector<Phase>& phases,const std::string& name) {
  for(const auto& p:phases)if(p.name==name)return p;
  require(false,"DDGI transition metric reference absent");return phases.front();
}
// Channels include real shader receivers and fixed probe buffer cohorts.
double value(const Sample& s,unsigned channel) {
  if(channel<3)return energy(s.receiver[channel]);
  if(channel<7) {const auto& c=s.cohort[(channel-3)/2];return energy((channel-3)%2?c.published:c.raw);}
  const unsigned i=channel-7;return energy(s.pixels[i/Receivers][i%Receivers]);
}
std::string channel_name(unsigned channel) {
  static const char* names[]={"hierarchy","fine_receiver","coarse_receiver","fine_raw","fine_published","coarse_raw","coarse_published"};
  if(channel<7)return names[channel];
  const unsigned i=channel-7;return std::string(names[i/Receivers])+"_pixel_"+std::to_string(i%Receivers);
}
double mean_tail(const Phase& p,unsigned channel) {
  double sum=0;unsigned n=0;
  for(const auto& s:p.samples)if(s.elapsed>=p.samples.back().elapsed-TailSeconds) {sum+=value(s,channel);++n;}
  require(n>=3,"DDGI transition reference tail needs three measured samples");return sum/n;
}
struct Metrics {
  double from{},target{},signal{},first80=-1,settled=-1,total_variation{},wrong_way{},overshoot{},undershoot{};
  double minimum=std::numeric_limits<double>::max(),maximum{},tail_variation{},max_adjacent{},blackout_seconds{};
  double max_relative_jump{},detrended_variation{},rms_error{};
  unsigned reversals{},control_mismatches{};bool signal_measurable{},clip_pass=true,settle_pass=true,oscillation_pass=true;
};
Metrics measure(const Phase& p,unsigned channel,double from,double target) {
  Metrics m;m.from=from;m.target=target;m.signal=std::abs(target-from);
  const double scale=std::max({std::abs(from),std::abs(target),.0001});
  const double tolerance=std::max(.0001,m.signal*.1),deadband=std::max(.00005,m.signal*.02);
  m.signal_measurable=m.signal>std::max(.0002,scale*.02);
  const int direction=target>=from?1:-1;int previous_direction=0;
  double previous=from,previous_time=0,previous_slope=0;
  for(unsigned i=0;i<p.samples.size();++i) {
    const auto& s=p.samples[i];const double v=value(s,channel),delta=v-previous;
    m.minimum=std::min(m.minimum,v);m.maximum=std::max(m.maximum,v);
    m.total_variation+=std::abs(delta);m.max_adjacent=std::max(m.max_adjacent,std::abs(delta));
    m.max_relative_jump=std::max(m.max_relative_jump,std::abs(delta)/scale);
    m.wrong_way+=std::max(0.,-direction*delta);
    m.overshoot=std::max(m.overshoot,direction*(v-target));
    m.undershoot=std::max(m.undershoot,-direction*(v-from));
    const int sign=delta>deadband?1:delta<-deadband?-1:0;
    if(sign) {m.reversals+=previous_direction!=0&&sign!=previous_direction;previous_direction=sign;}
    if(s.elapsed>=p.samples.back().elapsed-TailSeconds)m.tail_variation+=std::abs(delta);
    const double dt=s.elapsed-previous_time;
    if(v<std::min(from,target)*.5)m.blackout_seconds+=dt;
    if(i>1)m.detrended_variation+=std::abs(delta-previous_slope*dt);
    previous_slope=delta/std::max(dt,1e-6);
    const double progress=p.kind=="motion"?s.elapsed/p.samples.back().elapsed:1;
    const double error=v-(from+(target-from)*progress);m.rms_error+=error*error;
    if(m.first80<0&&std::abs(v-target)<=std::max(.0001,m.signal*.2))m.first80=s.elapsed;
    previous=v;previous_time=s.elapsed;
    for(const auto& c:s.cohort)m.control_mismatches+=c.control_mismatches;
  }
  m.rms_error=std::sqrt(m.rms_error/p.samples.size());
  // All following samples must remain in band for at least half a real second.
  for(unsigned i=0;i+2<p.samples.size();++i) {
    if(p.samples.back().elapsed-p.samples[i].elapsed<SettleWindow)break;
    bool stable=true;
    for(unsigned j=i;j<p.samples.size();++j)stable&=std::abs(value(p.samples[j],channel)-target)<=tolerance;
    if(stable) {m.settled=p.samples[i].elapsed;break;}
  }
  m.clip_pass=m.control_mismatches==0&&m.blackout_seconds==0;
  if(p.kind=="step") {
    m.settle_pass=m.settled>=0&&m.settled<=6;
    m.oscillation_pass=!m.signal_measurable||(m.wrong_way<=m.signal*.5&&m.overshoot<=m.signal*.25&&m.undershoot<=m.signal*.25);
  } else if(p.kind=="motion") {
    // The sun's physical response need not be monotonic through noon.
    m.oscillation_pass=m.max_relative_jump<=.35&&m.detrended_variation<=scale*2;
  }
  return m;
}
}
RGB tail(const Phase& p,unsigned channel) {
  RGB out{};unsigned count=0;
  for(const auto& s:p.samples)if(s.elapsed>=p.samples.back().elapsed-TailSeconds) {
    for(unsigned c=0;c<3;++c)out[c]+=s.receiver[channel][c];++count;
  }
  require(count>=3,"DDGI transition stable tail absent");for(double& c:out)c/=count;return out;
}
bool report(const std::filesystem::path& directory,const std::string& backend,const std::vector<Phase>& phases) {
  std::ofstream csv(directory/(backend+"-metrics.csv")),json(directory/(backend+"-summary.json"));
  require(bool(csv)&&bool(json),"DDGI transition metrics output");csv<<std::setprecision(9);json<<std::setprecision(9);
  csv<<"phase,kind,channel,from,target,signal,first80_seconds,sustained90_seconds,total_variation,wrong_way_energy,reversals,overshoot,undershoot,min,max,tail_variation,max_adjacent,max_relative_jump,blackout_seconds,detrended_variation,rms_reference_error,control_mismatches,clip_pass,settle_pass,oscillation_pass,enforced\n";
  json<<"{\n  \"backend\": \""<<backend<<"\",\n  \"production_gpu_receivers\": true,\n  \"clock\": \"steady_clock\",\n  \"thresholds\": {\"settle_seconds\": 6, \"settle_band\": 0.1, \"hold_seconds\": 0.5, \"wrong_way_signal_fraction\": 0.5, \"overshoot_signal_fraction\": 0.25, \"blackout_baseline_fraction\": 0.5},\n  \"metrics\": [\n";
  bool passed=true,first=true;unsigned gated=0,failed=0;
  const double baseline=mean_tail(find(phases,"ambient_sun_baseline"),0);
  const double torch=mean_tail(find(phases,"torch_added"),0);
  const bool signal=baseline>.0002&&torch>baseline+.0002;passed&=signal;
  for(const auto& p:phases) {
    if(p.from.empty())continue;
    for(unsigned channel=0;channel<7+3*Receivers;++channel) {
      const double from=mean_tail(find(phases,p.from),channel),target=mean_tail(find(phases,p.target),channel);
      const auto m=measure(p,channel,from,target);
      // Physical coarse visibility may be zero inside the fine room. It remains
      // recorded; hierarchy receiver pixels carry strict visible-output gates.
      const bool visible=channel==0||(channel>=7&&channel<7+Receivers);
      const bool enforce=p.strict&&visible;
      const bool ok=m.clip_pass&&m.settle_pass&&m.oscillation_pass;
      if(enforce) {++gated;if(!ok)++failed;passed&=ok;}
      passed&=m.control_mismatches==0;
      const auto name=channel_name(channel);
      csv<<p.name<<','<<p.kind<<','<<name<<','<<from<<','<<target<<','<<m.signal<<','<<m.first80<<','<<m.settled<<','<<m.total_variation<<','<<m.wrong_way<<','<<m.reversals<<','<<m.overshoot<<','<<m.undershoot<<','<<m.minimum<<','<<m.maximum<<','<<m.tail_variation<<','<<m.max_adjacent<<','<<m.max_relative_jump<<','<<m.blackout_seconds<<','<<m.detrended_variation<<','<<m.rms_error<<','<<m.control_mismatches<<','<<m.clip_pass<<','<<m.settle_pass<<','<<m.oscillation_pass<<','<<enforce<<'\n';
      if(!first)json<<",\n";first=false;
      json<<"    {\"phase\":\""<<p.name<<"\",\"channel\":\""<<name<<"\",\"kind\":\""<<p.kind<<"\",\"from\":"<<from<<",\"target\":"<<target
        <<",\"first80_seconds\":"<<m.first80<<",\"sustained90_seconds\":"<<m.settled<<",\"wrong_way_energy\":"<<m.wrong_way<<",\"reversals\":"<<m.reversals
        <<",\"overshoot\":"<<m.overshoot<<",\"undershoot\":"<<m.undershoot<<",\"total_variation\":"<<m.total_variation<<",\"tail_variation\":"<<m.tail_variation
        <<",\"blackout_seconds\":"<<m.blackout_seconds<<",\"max_relative_jump\":"<<m.max_relative_jump<<",\"detrended_variation\":"<<m.detrended_variation
        <<",\"control_mismatches\":"<<m.control_mismatches<<",\"clip_pass\":"<<(m.clip_pass?"true":"false")<<",\"settle_pass\":"<<(m.settle_pass?"true":"false")
        <<",\"oscillation_pass\":"<<(m.oscillation_pass?"true":"false")<<",\"enforced\":"<<(enforce?"true":"false")<<"}";
    }
  }
  json<<"\n  ],\n  \"retained_lighting_and_torch_signal\": "<<(signal?"true":"false")<<",\n  \"gated_metrics\": "<<gated<<",\n  \"failed_metrics\": "<<failed<<",\n  \"passed\": "<<(passed?"true":"false")<<"\n}\n";
  std::printf("ddgi_transition_metrics gated=%u failed=%u retained_signal=%u output=%s\n",gated,failed,unsigned(signal),directory.string().c_str());
  return passed;
}
}
