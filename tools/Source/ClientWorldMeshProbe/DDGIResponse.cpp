#include "Probe.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <thread>

namespace mesh_probe {
namespace {
using Clock=std::chrono::steady_clock;
using RGB=std::array<double,3>;
constexpr double PhaseSeconds=6,SampleSeconds=.1,TailSeconds=.6;
constexpr unsigned CohortSize=48;
double seconds(Clock::time_point start) {return std::chrono::duration<double>(Clock::now()-start).count();}
double energy(const RGB& rgb) {return (rgb[0]+rgb[1]+rgb[2])/3;}
struct Sample {
  double elapsed{},oldest_trace_seconds{};RGB rgb{};unsigned valid{},refreshed{};
  unsigned minimum_trace_frame=std::numeric_limits<unsigned>::max();
};
struct Phase {RGB rgb{};std::vector<Sample> samples;};
unsigned material(const Fixture& f,const char* name) {
  const std::string wanted=std::string("octaryn.basegame.block.")+name;
  for(unsigned i=0;i<f.catalog.size();++i)if(f.catalog[i].id==wanted)return i;
  require(false,"DDGI response fixture material missing");return 0;
}
class Response {
  WorldRenderer& r;
  std::uint64_t serial{};
  std::ofstream csv,summary;
  Clock::time_point epoch=Clock::now();
  std::vector<double> trace_completion_seconds{0};
public:
  explicit Response(Fixture& f):r(f.renderer) {
    std::filesystem::path directory="logs/client/ddgi-response";
    if(const char* path=SDL_getenv("OCTARYN_DDGI_RESPONSE_OUTPUT"))directory=path;
    std::filesystem::create_directories(directory);
    const std::string backend=r.device->getDeviceType()==rhi::DeviceType::D3D12?"d3d12":"vulkan";
    csv.open(directory/(backend+"-samples.csv"));summary.open(directory/(backend+"-phases.csv"));
    require(bool(csv)&&bool(summary),"DDGI response evidence files");
    csv<<"phase,elapsed_seconds,total_seconds,frame,slot,red,green,blue,valid_cohort,cohort_size,refreshed_cohort,minimum_trace_frame,cohort_oldest_trace_seconds,fine_pending,fine_oldest_seconds,coarse_pending,coarse_oldest_seconds,ray_pending,ray_jobs\n";
    summary<<"phase,red,green,blue,energy,first_80_percent_seconds,final_elapsed_seconds\n";
    scene(0,0,.5);
    require(world_ray_initialize(r)&&world_ray_available(r),"DDGI response requires production ray queries");
    require(world_local_lighting_initialize(r)&&world_ddgi_initialize(r),"DDGI response production initialization");
    require(open_world_renderer_set_ddgi_range(&r,16,128),"DDGI response saved radii");
    require(r.ddgi.available&&r.ddgi.fine_volume&&r.ddgi.fine_volume->available,"DDGI response both volumes required");
  }
  void scene(float sun,float ambient,double day) {
    auto settings=lighting_settings_default_value();settings.sun_strength=sun;settings.ambient_strength=ambient;
    open_world_renderer_set_lighting(&r,settings);
    WorldSceneSettings scene;scene.day_fraction=day;scene.clouds=false;scene.fog=false;
    open_world_renderer_set_scene(&r,scene);
  }
  void lights(const std::vector<WorldLocalLight>& lights) {
    require(open_world_renderer_set_lights(&r,lights.data(),unsigned(lights.size())),"DDGI response public lights");
  }
  void publish(StreamColumn& source) {
    source.blocks.compact();++source.revision;
    require(open_world_renderer_update(&r,source),"DDGI response public geometry publication");
  }
  void frame() {
    const auto start=Clock::now();const unsigned slot=r.frame_queue.slot(serial++);
    require(r.frame_queue.wait(slot,10000),"DDGI response frame wait");r.active_frame=slot;r.frames=serial;
    const WorldCamera camera{16,5,16,0,0,1.05f};r.temporal.camera=camera;
    world_renderer_prepare_draw(r,camera);world_block_lights_update(r);
    auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"DDGI response encoder");
    require(world_ray_prepare(r,commands,slot),"DDGI response production ray scene");
    require(world_local_lighting_prepare(r,commands),"DDGI response immutable light upload");
    require(world_ddgi_update(r,commands),"DDGI response production scheduler and shaders");
    auto submission=commands->finish();require(submission!=nullptr,"DDGI response command finish");
    require(r.frame_queue.submit(r.queue,submission,slot),"DDGI response submit");
    require(r.frame_queue.wait(slot,10000),"DDGI response completed GPU frame");
    const auto frame=r.ddgi.fine_volume->frame;
    trace_completion_seconds.resize(frame+1);
    trace_completion_seconds[frame]=seconds(epoch);
    std::this_thread::sleep_until(start+std::chrono::microseconds(16667));
  }
  Sample read(double elapsed,std::uint64_t phase_start_frame) {
    auto& s=*r.ddgi.fine_volume;
    const auto in_cohort=[](const DDGIControl& c) {
      return c.cell[0]>=14&&c.cell[0]<18&&c.cell[1]>=3&&c.cell[1]<6&&c.cell[2]>=14&&c.cell[2]<18;
    };
    unsigned begin=s.stats.probe_count,end=0;
    for(unsigned i=0;i<s.control_data.size();++i)if(in_cohort(s.control_data[i])) {
      begin=std::min(begin,i);end=std::max(end,i+1);
    }
    require(begin<end,"DDGI response cohort absent");
    // Read only the cohort's enclosing span; whole-volume readbacks distort cadence.
    std::vector<DDGIProbe> probes(end-begin);
    const unsigned texels=s.config.irradiance_resolution*s.config.irradiance_resolution;
    std::vector<std::array<float,4>> values(std::size_t(end-begin)*texels);
    checked(r.device->readBuffer(s.probes,std::size_t(begin)*sizeof(DDGIProbe),probes.size()*sizeof(DDGIProbe),probes.data()),"DDGI response probe readback");
    checked(r.device->readBuffer(s.irradiance,std::size_t(begin)*texels*sizeof(values[0]),values.size()*sizeof(values[0]),values.data()),"DDGI response irradiance readback");
    Sample result;result.elapsed=elapsed;unsigned cohort=0;
    for(unsigned i=0;i<probes.size();++i) {
      const auto& c=s.control_data[begin+i];const auto& p=probes[i];
      if(!in_cohort(c))continue;
      ++cohort;
      require(p.metadata[1]<trace_completion_seconds.size(),"DDGI response GPU trace frame exceeds completed frame");
      result.minimum_trace_frame=std::min(result.minimum_trace_frame,p.metadata[1]);
      result.oldest_trace_seconds=std::max(result.oldest_trace_seconds,seconds(epoch)-trace_completion_seconds[p.metadata[1]]);
      if(p.metadata[0]!=c.version||p.metadata[3]==0||p.metadata[1]==0||p.offset[3]==1||c.padding[0]!=0||c.padding[1]!=0||
          (c.padding[2]==2&&c.refresh_frame>p.metadata[1]))continue;
      ++result.valid;
      result.refreshed+=p.metadata[1]>phase_start_frame;
      for(unsigned t=0;t<texels;++t)for(unsigned channel=0;channel<3;++channel) {
        const float value=values[i*texels+t][channel];
        require(std::isfinite(value)&&value>=0,"DDGI response invalid irradiance");
        result.rgb[channel]+=value/double(CohortSize*texels);
      }
    }
    require(cohort==CohortSize,"DDGI response fixed spatial cohort changed");return result;
  }
  Phase phase(const std::string& name) {
    const auto start=Clock::now();double next=0;Phase result;
    const auto phase_start_frame=r.ddgi.fine_volume->frame;
    do {
      frame();const double elapsed=seconds(start);if(elapsed<next)continue;next=elapsed+SampleSeconds;
      auto sample=read(elapsed,phase_start_frame);result.samples.push_back(sample);
      const auto& fine=r.ddgi.fine_volume->stats;const auto& coarse=r.ddgi.stats;const auto rays=world_ray_stats(r);
      csv<<name<<','<<elapsed<<','<<seconds(epoch)<<','<<serial<<','<<r.active_frame;
      for(double value:sample.rgb)csv<<','<<value;
      csv<<','<<sample.valid<<','<<CohortSize<<','<<sample.refreshed<<','<<sample.minimum_trace_frame<<','<<sample.oldest_trace_seconds
        <<','<<fine.pending_probes<<','<<fine.oldest_update_seconds
        <<','<<coarse.pending_probes<<','<<coarse.oldest_update_seconds<<','<<rays.pending_columns<<','<<rays.active_jobs<<'\n';
      csv.flush();
    } while(seconds(start)<PhaseSeconds);
    unsigned count=0;
    for(const auto& sample:result.samples)if(sample.elapsed>=PhaseSeconds-TailSeconds) {
      require(sample.valid==CohortSize,"DDGI response tail has missing/pending/embedded cohort probes");
      require(sample.refreshed==CohortSize,"DDGI response cohort retained frozen traces across an elapsed-time phase");
      for(unsigned c=0;c<3;++c)result.rgb[c]+=sample.rgb[c];++count;
    }
    require(count>=3,"DDGI response GPU too slow for a measured stable tail");
    for(auto& value:result.rgb)value/=count;
    const auto rays=world_ray_stats(r);
    require(rays.ready_columns==1&&rays.pending_columns==0&&rays.active_jobs==0,"DDGI response geometry not published by deadline");
    require(r.debug.errors.load()==0,"DDGI response graphics validation errors");
    std::printf("ddgi_response phase=%s energy=%.7f rgb=%.7f,%.7f,%.7f elapsed=%.3f cohort=%u\n",
      name.c_str(),energy(result.rgb),result.rgb[0],result.rgb[1],result.rgb[2],result.samples.back().elapsed,CohortSize);
    return result;
  }
  void report(const std::string& name,const Phase& phase,const RGB& from) {
    double first=-1,delta=0;for(unsigned c=0;c<3;++c)delta+=std::abs(phase.rgb[c]-from[c]);
    for(std::size_t i=0;i+2<phase.samples.size();++i) {
      bool reached=true;
      for(unsigned j=0;j<3;++j) {
        const auto& sample=phase.samples[i+j];double error=0;
        for(unsigned c=0;c<3;++c)error+=std::abs(sample.rgb[c]-phase.rgb[c]);
        reached&=sample.valid==CohortSize&&sample.refreshed==CohortSize&&error<=std::max(.0001,delta*.2);
      }
      if(reached) {first=phase.samples[i].elapsed;break;}
    }
    summary<<name;for(double value:phase.rgb)summary<<','<<value;
    summary<<','<<energy(phase.rgb)<<','<<first<<','<<phase.samples.back().elapsed<<'\n';summary.flush();
    std::printf("ddgi_response_latency phase=%s first_80_percent_seconds=%.3f bound_seconds=%.1f\n",name.c_str(),first,PhaseSeconds);
    require(first>=0&&first<=PhaseSeconds,"DDGI response did not sustain 80 percent response within elapsed-time bound");
  }
};
void brighter(const Phase& cold,const Phase& lit,const char* message) {
  require(energy(lit.rgb)>energy(cold.rgb)+.002,message);
}
void restored(const Phase& cold,const Phase& lit,const Phase& removed,const char* message) {
  const double signal=energy(lit.rgb)-energy(cold.rgb);
  require(signal>.002,message);
  require(std::abs(energy(removed.rgb)-energy(cold.rgb))<=std::max(.0002,signal*.2),message);
}
WorldLocalLight light(unsigned type) {
  WorldLocalLight l;l.position_range={16,7,16,16};l.color_intensity={1,.02f,.02f,100};
  l.direction_outer={0,-1,0,.3f};l.axis_u_inner={1,0,0,.8f};l.axis_v_type={0,0,1,float(type)};return l;
}
void local_cases(Response& probe,const Phase& cold) {
  const char* names[]={"point","spot","area"};
  for(unsigned type=0;type<3;++type) {
    const std::string name=names[type];auto l=light(type);probe.lights({l});
    const auto red=probe.phase(name+"_red");brighter(cold,red,"DDGI local light did not reach probes");probe.report(name+"_red",red,cold.rgb);
    l.color_intensity={.02f,.02f,1,100};probe.lights({l});
    const auto blue=probe.phase(name+"_blue");
    require(blue.rgb[2]>blue.rgb[0]*2&&red.rgb[0]>red.rgb[2]*2,"DDGI local color retained old hue");probe.report(name+"_blue",blue,red.rgb);
    l.color_intensity[3]=25;probe.lights({l});const auto dim=probe.phase(name+"_dim");
    require(energy(dim.rgb)<energy(blue.rgb)*.6&&energy(dim.rgb)>energy(cold.rgb)+.0002,"DDGI intensity change failed");probe.report(name+"_dim",dim,blue.rgb);
    l.position_range[0]=80;probe.lights({l});const auto moved=probe.phase(name+"_moved");
    restored(cold,dim,moved,"DDGI moved light left energy at old location");probe.report(name+"_moved",moved,dim.rgb);
    l=light(type);probe.lights({l});const auto replaced=probe.phase(name+"_returned");brighter(cold,replaced,"DDGI returned light missing");
    probe.lights({});const auto removed=probe.phase(name+"_removed");
    restored(cold,replaced,removed,"DDGI removed light left ghost energy");probe.report(name+"_removed",removed,replaced.rgb);
  }
}
}
void ddgi_response_cases(Fixture& f) {
  Response probe(f);auto source=column();const auto stone=std::uint16_t(material(f,"stone"));
  for(int z=0;z<32;++z)for(int x=0;x<32;++x)put(source,x,1,z,stone);
  for(int y=2;y<10;++y)for(int i=10;i<22;++i) {
    put(source,10,y,i,stone);put(source,21,y,i,stone);put(source,i,y,10,stone);put(source,i,y,21,stone);
  }
  probe.publish(source);const auto cold=probe.phase("cold_floor");
  local_cases(probe,cold);
  probe.scene(2,0,.5);const auto sun=probe.phase("sun_on");brighter(cold,sun,"DDGI sun strength had no response");probe.report("sun_on",sun,cold.rgb);
  for(int z=10;z<22;++z)for(int x=10;x<22;++x)put(source,x,10,z,stone);
  probe.publish(source);const auto closed=probe.phase("opaque_roof_closed");
  restored(cold,sun,closed,"DDGI opaque closure retained sunlight");probe.report("opaque_roof_closed",closed,sun.rgb);
  for(int z=10;z<22;++z)for(int x=10;x<22;++x)put(source,x,10,z,0);
  probe.publish(source);const auto opened=probe.phase("opaque_roof_opened");
  require(energy(opened.rgb)>energy(sun.rgb)*.6,"DDGI opening did not restore sunlight");probe.report("opaque_roof_opened",opened,closed.rgb);
  for(int z=10;z<22;++z)for(int x=0;x<12;++x)put(source,x,12,z,stone);
  probe.publish(source);const auto parked=probe.phase("roof_parked");
  const auto parkedCounts=f.renderer.columns.begin()->second.pass_counts;
  for(int z=10;z<22;++z)for(int x=0;x<12;++x)put(source,x,12,z,0);
  for(int z=10;z<22;++z)for(int x=10;x<22;++x)put(source,x,12,z,stone);
  probe.publish(source);const auto shifted=probe.phase("equal_count_roof_shift");
  require(f.renderer.columns.begin()->second.pass_counts==parkedCounts,"roof shift must preserve mesh counts");
  require(energy(shifted.rgb)<energy(parked.rgb)*.2,"equal-count roof shift retained sunlight");
  probe.report("equal_count_roof_shift",shifted,parked.rgb);
  for(int z=10;z<22;++z)for(int x=10;x<22;++x)put(source,x,12,z,0);
  probe.publish(source);probe.phase("shifted_roof_removed");
  // Reported scene: a canopy above a sealed stone roof must not project sunlight
  // into the dark interior, regardless of sun angle through the leaves.
  const auto leaves=std::uint16_t(material(f,"leaves"));
  for(int z=10;z<22;++z)for(int x=10;x<22;++x)put(source,x,10,z,stone);
  for(int z=10;z<22;++z)for(int x=10;x<22;++x)put(source,x,12,z,leaves);
  probe.publish(source);const auto canopySealed=probe.phase("canopy_over_sealed_roof");
  require(energy(canopySealed.rgb)<energy(closed.rgb)*1.2f+.001f,
    "canopy admitted sunlight through a sealed stone roof");
  probe.report("canopy_over_sealed_roof",canopySealed,opened.rgb);
  for(int z=10;z<22;++z)for(int x=10;x<22;++x)put(source,x,12,z,0);
  for(int z=10;z<22;++z)for(int x=10;x<22;++x)put(source,x,10,z,0);
  probe.publish(source);probe.phase("canopy_removed");
  probe.scene(0,0,.5);const auto noSun=probe.phase("sun_off");restored(cold,sun,noSun,"DDGI sun removal retained energy");probe.report("sun_off",noSun,sun.rgb);
  probe.scene(0,1,0);const auto night=probe.phase("night");
  probe.scene(0,1,.5);const auto day=probe.phase("day");brighter(night,day,"DDGI daylight had no response");probe.report("day",day,night.rgb);
  probe.scene(0,1,0);const auto nightAgain=probe.phase("night_returned");restored(night,day,nightAgain,"DDGI night retained daylight");probe.report("night_returned",nightAgain,day.rgb);
  probe.scene(0,0,.5);probe.phase("emission_cold");
  const auto lava=std::uint16_t(material(f,"lava"));
  for(int z=12;z<20;++z)for(int x=12;x<20;++x)put(source,x,1,z,lava);
  probe.publish(source);const auto emission=probe.phase("resident_emission");brighter(cold,emission,"resident emissive material did not illuminate probes");probe.report("resident_emission",emission,cold.rgb);
  for(int z=12;z<20;++z)for(int x=12;x<20;++x)put(source,x,1,z,stone);
  probe.publish(source);const auto noEmission=probe.phase("resident_emission_removed");restored(cold,emission,noEmission,"resident emission removal retained energy");probe.report("resident_emission_removed",noEmission,emission.rgb);
  std::puts("ddgi_response=passed production_scheduler=1 wall_clock=1 production_ray_scene=1 gpu_irradiance=1 fine_radius=16 coarse_radius=128 point=1 spot=1 area=1 sun_strength=1 daylight=1 opaque_close_open=1 resident_emission=1 canopy_sealed=1 cohort=48 bound_seconds=6");
}
}
