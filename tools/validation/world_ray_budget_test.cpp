#include "WorldRayBuildBudget.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace octaryn::client::rendering::world_ray;
namespace {
void check(bool condition,const char* detail) {
  if(!condition) {std::fprintf(stderr,"FAILED: %s\n",detail);std::exit(1);}
}
unsigned dispatch(unsigned available,unsigned face_limit,unsigned faces,std::uint64_t cost_ns) {
  BuildBudget budget(available,face_limit);
  std::uint64_t elapsed{};
  while(budget.allows(faces,elapsed)) {
    budget.consumed(faces);elapsed+=cost_ns;
  }
  return budget.started;
}
}
int main() {
  check(dispatch(100,262144,1000,100000)==8,"fixed capacity bounds an unlimited backlog");
  check(dispatch(3,262144,1000,100000)==3,"busy GPU slots cannot be reused");
  check(dispatch(1,262144,1000,100000)==1,"explicit one-build budget remains supported");
  check(dispatch(0,262144,1000,100000)==0,"no free GPU slot starts no work");
  check(dispatch(8,262144,1000,700000)==3,"stop after the first indivisible CPU budget overrun");
  check(dispatch(8,262144,1000,2000000)==1,"exact CPU deadline stops further starts");
  check(dispatch(8,262144,1000,5000000)==1,"one slow allocation cannot create a burst");
  check(dispatch(8,100,40,0)==2,"cumulative faces cap the selected work");
  check(dispatch(8,100,50,0)==2,"exact face limit is accepted");
  check(dispatch(8,100,200,0)==1,"oversized first column still makes progress");
  check(dispatch(8,UINT32_MAX,UINT32_MAX,0)==1,"face accounting cannot wrap around");
  FrameBuildBudget frame;
  frame.reset(2,100);
  check(frame.allows(60,0),"frame-head first submission admitted");
  frame.consumed(60);frame.cpu_ns+=600000;
  check(!frame.allows(41,0) && frame.allows(40,0),"private slice preserves head face budget");
  frame.consumed(40);frame.cpu_ns+=400000;
  for(unsigned slice=0;slice<16;++slice)
    check(!frame.allows(1,0),"repeated private slices cannot refill per-frame count");
  frame.reset(8,100);
  frame.consumed(10);frame.cpu_ns+=1000000;
  check(frame.allows(10,999999) && !frame.allows(10,1000000),
      "head plus current private CPU time enforces exact shared deadline");
  frame.cpu_ns+=1000000;
  check(!frame.allows(10,0),"later private slice cannot reset consumed CPU budget");
  frame.reset(8,100);
  check(frame.allows(200,0),"one oversized first column still progresses after frame reset");
  frame.consumed(200);frame.cpu_ns+=5000000;
  check(!frame.allows(1,0),"oversized or slow first column cannot burst in private slices");
  frame.reset(0,100);check(!frame.allows(1,0),"zero frame submission admission remains zero");
  const std::array<unsigned,12> queued{300000,40,80,20,60,10,70,90,10,40,50,10};
  std::vector<unsigned> completed;
  unsigned next{},frames{};
  while(next<queued.size() && frames<queued.size()) {
    BuildBudget budget(8,100);
    std::uint64_t elapsed{};
    while(next<queued.size() && budget.allows(queued[next],elapsed)) {
      completed.push_back(next);budget.consumed(queued[next++]);elapsed+=700000;
    }
    ++frames;
  }
  check(next==queued.size() && frames>1,"deferred near-first work eventually drains");
  for(unsigned index=0;index<completed.size();++index)
    check(completed[index]==index,"budget deferral preserves candidate order");
  std::printf("world_ray_budget=passed capacity=%u cpu_budget_ms=%.3f face_limit=preserved bounded=passed ordered=passed oversized=passed shared_private_budget=passed\n",
      BuildJobCapacity,double(BuildCpuBudgetNs)/1e6);
}
