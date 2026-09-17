#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace octaryn::client::world_presentation {
struct TossPose {float x{},y{},z{},yaw{},pitch{};};
struct ProvisionalToss {
 std::uint64_t request{},command{};
 std::uint32_t block{},count{};
 float x{},y{},z{},vx{},vy{},vz{};
 double age{};
};
inline bool valid_toss_pose(const TossPose& p) {
 return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
  std::isfinite(p.yaw)&&std::isfinite(p.pitch);
}
inline ProvisionalToss make_provisional_toss(std::uint64_t request,std::uint64_t command,
 std::uint32_t block,std::uint32_t count,const TossPose& p) {
 return {request,command,block,count,p.x,p.y-.25f,p.z,
  std::sin(p.yaw)*std::cos(p.pitch)*4,std::sin(p.pitch)*4+2,
  -std::cos(p.yaw)*std::cos(p.pitch)*4,0};
}
inline void advance_provisional_toss(ProvisionalToss& p,double seconds) {
 // Cosmetic free flight follows the authoritative gravity/drag; no world queries.
 while(seconds>0) {
  const float dt=static_cast<float>(std::min(seconds,1.0/120));
  p.vy=std::max(-32.f,p.vy-20*dt);
  p.x+=p.vx*dt;p.y+=p.vy*dt;p.z+=p.vz*dt;
  const float drag=std::exp(-.6f*dt);p.vx*=drag;p.vz*=drag;
  p.age+=dt;seconds-=dt;
 }
}
}
