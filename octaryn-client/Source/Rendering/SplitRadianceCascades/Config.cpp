#include "Config.h"
#include <cmath>
#include <limits>
namespace octaryn::client::rendering {
bool src_validate(const SrcConfig& c,SrcLayout& out,std::string& error) {
  auto reject=[&](const char* text){error=text;return false;};
  for(float v:{c.spacing,c.contact_length,c.base_interval,c.interval_growth,c.decay,c.max_trace_distance,c.feedback,c.lod_radius,c.lod_blend})
    if(!std::isfinite(v))return reject("SRC settings must be finite");
  if(c.spacing<=0||c.contact_length<0||c.base_interval<=c.contact_length||c.interval_growth<=1||
     c.decay<=0||c.decay>1||c.feedback<0||c.feedback>=1||c.max_trace_distance<=c.base_interval*c.spacing||c.lod_radius<=0||c.lod_blend<=1||c.lod_blend>2)
    return reject("SRC interval, decay or feedback range is invalid");
  if(!c.cascades||c.cascades>src_max_cascades||!c.base_capacity||
     (c.base_capacity&(c.base_capacity-1))||!c.angular_resolution||
     (c.angular_resolution&(c.angular_resolution-1))||c.angular_resolution>16||
     !c.visible_lifetime||!c.secondary_lifetime||!c.max_surface_rays||c.max_surface_rays>16777216||
     !c.hash_search_limit||c.hash_search_limit>4096)
    return reject("SRC capacity, cascade, lifetime or search bound is invalid");
  SrcLayout l{};std::uint64_t p=0,d=0,h=0;
  if(c.spacing<1||c.spacing>16||std::floor(std::log2(c.spacing))!=std::log2(c.spacing))
    return reject("SRC spacing must be an integer power of two from 1 to 16 for exact rebased keys");
  for(unsigned i=0;i<c.cascades;++i) {
    const std::uint64_t capacity=c.base_capacity>>(2*i),resolution=std::uint64_t(c.angular_resolution)<<i;
    const std::uint64_t directions=2*resolution*resolution;
    if(!capacity)return reject("SRC cascade capacity becomes zero");
    if(d+capacity*directions>std::numeric_limits<unsigned>::max()||h+capacity*4>std::numeric_limits<unsigned>::max())
      return reject("SRC layout exceeds 32-bit shader indexing");
    l.cascades[i]={unsigned(p),unsigned(capacity),unsigned(d),unsigned(directions),unsigned(h),unsigned(capacity*4),unsigned(resolution),0};
    p+=capacity;d+=capacity*directions;h+=capacity*4;
  }
  l.probes=unsigned(p);l.directions=unsigned(d);l.hash_entries=unsigned(h);
  // Probe=64, freelist=4, links=64, distribution=16, hash=4, interval=16, weight=4, histories=2*16, ray=80.
  l.bytes=p*148+h*4+d*52+p*36*32+std::uint64_t(c.max_surface_rays)*80+256;
  if(l.bytes>c.memory_limit)return reject("SRC persistent allocation exceeds memory limit");
  out=l;error.clear();return true;
}
}
