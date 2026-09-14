#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
using uint=std::uint32_t;
struct float2 {float x{},y{};};
using std::isfinite;
using std::min;
// Exercise the exact Slang production reservoir arithmetic on the CPU.
#include "../../octaryn-client/Shaders/Lighting/Reservoir.slang"
int main() {
  uint rng=7891;Reservoir zero{};
  reservoir_add(zero,0,{},0,0,1,0,0,rng);
  if(zero.M!=1 || zero.weightSum!=0 || reservoir_weight(zero)!=0)return 1;
  reservoir_add(zero,0,{},1,std::numeric_limits<float>::infinity(),1,0,0,rng);
  if(zero.M!=1)return 2;
  constexpr uint trials=250000;
  double sum=0;unsigned chosen=0;
  for(uint i=0;i<trials;++i) {
    Reservoir r{};
    reservoir_add(r,0,{},1,2,1,0,0,rng);
    reservoir_add(r,1,{},3,6,1,0,0,rng);
    chosen+=r.light;
    sum+=(r.light?3:1)*reservoir_weight(r);
  }
  if(std::abs(double(chosen)/trials-.75)>.004 || std::abs(sum/trials-4)>1e-5)return 3;
  Reservoir source{};reservoir_add(source,2,{},4,20,10,0,0,rng);
  Reservoir merged{};reservoir_merge(merged,source,8,3,1,rng);
  if(merged.M!=3 || std::abs(merged.weightSum-12)>1e-5 || std::abs(reservoir_weight(merged)-.5)>1e-5)return 4;
  // Re-evaluation at a different surface preserves the new destination integral.
  sum=0;
  for(uint i=0;i<trials;++i) {
    Reservoir a{};
    for(uint c=0;c<8;++c) {
      uint light=restir_random(rng)<.5?0:1;
      float target=light?3.f:1.f;
      reservoir_add(a,light,{},target,target*2,1,0,0,rng);
    }
    Reservoir b{};float target=a.light?2.f:5.f;
    reservoir_merge(b,a,target,4,1,rng);
    sum+=target*reservoir_weight(b);
  }
  if(std::abs(sum/trials-7)>.035)return 5;
  std::printf("restir_math=passed trials=%u weighted_selection=%.6f destination_integral=%.6f\n",trials,double(chosen)/trials,sum/trials);
  return 0;
}
