#include "DDGIVolumeConfig.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>

using namespace octaryn::client::rendering;
static void require(bool condition,const char* message) {
  if(!condition)throw std::runtime_error(message);
}
static std::uint64_t bytes(const DDGIConfig& c) {
  const unsigned probes=c.counts[0]*c.counts[1]*c.counts[2];
  const unsigned capacity=std::min(probes,std::max(c.budget,4096u));
  return std::uint64_t(probes)*(sizeof(DDGIControl)+sizeof(DDGIProbe)+4+
    c.irradiance_resolution*c.irradiance_resolution*16+c.visibility_resolution*c.visibility_resolution*8)+
    std::uint64_t(capacity)*(c.rays*32+16);
}
static void initialize(DDGISystem& s,unsigned radius,bool fine) {
  DDGIConfig base;base.rays=176;
  s.config=ddgi_volume_config(base,radius,fine,8);
  const unsigned count=s.config.counts[0]*s.config.counts[1]*s.config.counts[2];
  s.control_data.resize(count);s.last_updates.resize(count,1);s.last_update_times.resize(count);
  s.dirty.resize(count,false);s.occupancy.resize(count,2);s.frame=1;
  ddgi_scroll(s,{0,0,0});
  std::fill(s.dirty.begin(),s.dirty.end(),false);std::fill(s.last_updates.begin(),s.last_updates.end(),1);
}
static void benchmark(unsigned radius,bool fine) {
  DDGISystem s;initialize(s,radius,fine);
  constexpr unsigned iterations=2000;
  auto begin=std::chrono::steady_clock::now();
  for(unsigned i=0;i<iterations;++i)ddgi_scroll(s,{float(i%10)*.001f,0,0});
  auto end=std::chrono::steady_clock::now();
  const double scroll=std::chrono::duration<double,std::micro>(end-begin).count()/iterations;
  begin=std::chrono::steady_clock::now();
  for(unsigned i=0;i<iterations;++i) {
    ++s.frame;s.time_seconds+=1./144;s.frame_seconds=1./144;
    ddgi_scroll(s,{0,0,0});ddgi_schedule(s,{0,0,0});
  }
  end=std::chrono::steady_clock::now();
  const double schedule=std::chrono::duration<double,std::micro>(end-begin).count()/iterations;
  std::printf("ddgi_cpu radius=%u fine=%u probes=%zu spacing=%.3f trace=%.3f bytes=%llu scroll_us=%.3f schedule_us=%.3f\n",
    radius,unsigned(fine),s.control_data.size(),s.config.spacing,s.config.max_distance,
    static_cast<unsigned long long>(bytes(s.config)),scroll,schedule);
}
int main(int argc,char** argv) {
  for(unsigned radius:{128u,256u,512u,1024u})benchmark(radius,false);
  benchmark(16,true);
  if(argc>1 && !std::strcmp(argv[1],"benchmark"))return 0;
  DDGIConfig base;base.rays=176;
  for(unsigned radius=1;radius<=1024;++radius) {
    const auto c=ddgi_volume_config(base,radius,false,8);
    require(c.counts[0]<=32 && c.counts[2]<=32 && c.counts[1]==12,"far range increased the probe allocation cap");
    require(c.counts[0]*c.spacing>=radius*2,"nominal horizontal range was truncated");
    require(c.max_distance>=radius*2,"trace distance cannot span the requested working area");
    require(c.rays==base.rays && c.hysteresis==base.hysteresis,"range changed ray quality or observation retention");
    require(bytes(c)<=37339136,"coarse allocation exceeded 35.609 MiB at default formats/rays");
  }
  for(unsigned radius=1;radius<=32;++radius) {
    const auto c=ddgi_volume_config(base,radius,true,8);
    require(c.counts==std::array<unsigned,3>{radius*2,radius*2,radius*2} && c.spacing==1,
      "fine range lost one-block probe density");
    require(c.max_distance==std::max(24.f,float(radius)*2),"fine trace distance changed");
  }
  const auto largest=ddgi_volume_config(base,std::numeric_limits<unsigned>::max(),false,8);
  require(largest.counts==std::array<unsigned,3>{32,12,32} && largest.spacing==64 && largest.max_distance==2048,
    "unchecked unsigned input escaped bounded configuration");
  auto maximum=base;maximum.rays=512;maximum.irradiance_resolution=maximum.visibility_resolution=16;
  require(bytes(ddgi_volume_config(maximum,1024,false,8))==143507456,
    "maximum environment quality exceeded the coarse memory bound");
  for(float spacing:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
    const auto c=ddgi_volume_config(base,128,false,spacing);
    require(std::isfinite(c.spacing) && c.spacing>0 && c.counts[0]<=32,"invalid base spacing escaped validation");
  }
  DDGISystem s;initialize(s,128,false);
  const auto previous=s.control_data;const auto fade=s.fade_origin;
  ddgi_scroll(s,{.25f,0,0});
  require(s.fade_origin[0]!=fade[0],"stationary-cell scroll stopped continuous camera fade");
  for(unsigned i=0;i<previous.size();++i)
    require(s.control_data[i].cell==previous[i].cell && s.control_data[i].version==previous[i].version,
      "subcell motion invalidated observation history");
  ddgi_scroll(s,{s.config.spacing,0,0});
  unsigned changed=0;
  for(unsigned i=0;i<previous.size();++i)changed+=s.control_data[i].version!=previous[i].version;
  require(changed==s.config.counts[1]*s.config.counts[2],"one-cell scroll failed to publish exactly the exposed plane");
  for(unsigned radius:{128u,256u,512u,1024u}) {
    DDGISystem volume;initialize(volume,radius,false);
    require(volume.control_data.size()==12288,"far range did not allocate the documented actual cell count");
    for(const auto& camera:std::array<std::array<float,3>,3>{{{0,0,0},{123.25f,37.5f,-72.75f},{-.01f,-.01f,-.01f}}}) {
      ddgi_scroll(volume,camera);
      std::set<std::array<int,3>> cells;
      for(const auto& control:volume.control_data)cells.insert(control.cell);
      for(float x:{-32.f,0.f,32.f})for(float y:{-32.f,0.f,32.f})for(float z:{-32.f,0.f,32.f}) {
        const std::array<float,3> receiver{camera[0]+x,camera[1]+y,camera[2]+z};
        for(unsigned corner=0;corner<8;++corner) {
          std::array<int,3> cell;
          for(unsigned axis=0;axis<3;++axis)
            cell[axis]=int(std::floor(receiver[axis]/volume.config.spacing))+int((corner>>axis)&1);
          require(cells.contains(cell),"far coarse grid lost a near-volume interpolation corner or height coverage");
        }
      }
    }
  }
  std::puts("ddgi_range_test=passed coarse_radii=1024 fine_radii=32 bounded_input=1 stationary_history=1 continuous_fade=1 exposed_plane=1 near_cage_positions=324");
}
