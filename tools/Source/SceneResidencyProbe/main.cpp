#include "SceneResidency.h"
#include "SceneBounds.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

using namespace octaryn::scene_geometry;
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
Instance instance(unsigned node,float x) {
  Instance out{};out.node=node;out.mesh=0;
  out.transform={1,0,0,0,0,1,0,0,0,0,1,0,x,0,0,1};out.bounds={x,0,0,x+101,1,1};return out;
}
void test() {
  std::vector<Part> parts(2);
  for(auto& part:parts) {part.triangle_count=100;part.cooked=part.bounds_prepared=true;part.reservation_bytes=100;}
  parts[0].bounds={0,0,0,1,1,1};parts[1].bounds={100,0,0,101,1,1};
  std::vector<Instance> nodes{instance(4,0),instance(7,10)};
  ResidencyIndex index;std::string error;require(index.reset(parts,nodes,error),error.c_str());
  Query query{};query.camera={5,0,0};query.actor=query.camera;query.actor_radius=0;query.load_radius=6;query.keep_radius=8;query.budget_bytes=100;
  auto plan=index.plan(query);
  require(plan.admitted && plan.wanted.size()==1 && plan.wanted[0].instances.size()==2 && plan.reservation_bytes==100,
      "instances must share one part reservation and exclude distant subparts");
  const auto shared_active=plan.wanted;query.camera=query.actor={2,0,0};query.load_radius=3;query.keep_radius=10;
  plan=index.plan(query,shared_active);
  require(plan.admitted && plan.wanted[0].instances.size()==1 && plan.retained.size()==1 &&
      plan.retained[0].instances==std::vector<unsigned>{1} && plan.reservation_bytes==100,"same-part instances must share hysteresis without double reservation");
  query.camera=query.actor={5,0,0};query.load_radius=6;query.keep_radius=8;
  query.actor={100,0,0};query.actor_radius=1;
  plan=index.plan(query);require(!plan.admitted && plan.wanted.size()==2 && plan.reservation_bytes==200,"protected actor admission must not truncate");
  query.budget_bytes=200;plan=index.plan(query);require(plan.admitted && plan.wanted.size()==2,"actor region must be admitted");
  parts[1].bounds_prepared=false;require(index.reset(parts,nodes,error),error.c_str());
  plan=index.plan(query);require(!plan.admitted && plan.pending_parts==std::vector<unsigned>{1},"unknown window bounds must remain pending");
  parts[1].bounds_prepared=true;parts[1].cooked=false;require(index.reset(parts,nodes,error),error.c_str());
  plan=index.plan(query);require(!plan.admitted && plan.pending_parts==std::vector<unsigned>{1},"uncooked source parts must remain pending");
  parts[1].cooked=true;nodes.resize(1);nodes[0].transform[0]=-2;nodes[0].transform[8]=3;nodes[0].transform[5]=4;
  nodes[0].transform[12]=20;nodes[0].transform[13]=100;
  require(index.reset(parts,nodes,error),error.c_str());
  query.camera={21,102,1};query.actor=query.camera;query.actor_radius=0;query.load_radius=1;query.keep_radius=4;
  plan=index.plan(query);require(plan.admitted && plan.wanted.size()==1 && plan.wanted[0].part==0,"mirrored sheared bounds must remain queryable");
  const auto active=plan.wanted;query.camera={25,102,1};query.actor=query.camera;
  plan=index.plan(query,active);require(plan.wanted.empty() && plan.retained.size()==1,"keep radius must preserve nearby resident geometry");
  query.camera={21,0,1};query.actor=query.camera;query.ignore_vertical=true;
  plan=index.plan(query);require(plan.admitted && plan.wanted.size()==1,"authority XZ query must preserve vertically separated collision");
  query.ignore_vertical=false;plan=index.plan(query);require(plan.wanted.empty(),"render query must retain vertical distance");
  query.region=Bounds{20,-32,0,21,104,1};
  plan=index.plan(query);require(plan.admitted && plan.wanted.size()==1 && plan.wanted[0].part==0,
      "narrow vertical ray region must preserve full depth without loading distant XZ geometry");
  query.region=Bounds{20,-32,0,21,99,1};plan=index.plan(query);require(plan.wanted.empty(),"bounds query ignored vertical limits");
  query.region=Bounds{20,100,0,21,104,1};query.keep_region=Bounds{19,99,-1,24,105,2};
  parts[0].bounds_prepared=false;require(index.reset(parts,nodes,error),error.c_str());
  plan=index.plan(query);require(!plan.admitted && plan.pending_parts==std::vector<unsigned>{0},"bounds query published unknown geometry");
  parts[0].bounds_prepared=true;require(index.reset(parts,nodes,error),error.c_str());query.budget_bytes=99;
  require(!index.plan(query).admitted,"bounds query bypassed complete budget admission");query.budget_bytes=200;
  query.keep_region=Bounds{20,101,0,21,104,1};require(!index.plan(query).admitted,"keep region omitted required geometry");
  query.region.reset();query.keep_region.reset();
  auto cancellation=instance(11,0).transform;cancellation[0]=1e8f;cancellation[4]=-1e8f;
  const auto padded=bounds::transform({1.000001f,1,0,1.000001f,1,0},cancellation);
  volatile float product=1e8f*1.000001f;
  const float gpu_x=product-1e8f;
  require(padded[0]<=gpu_x && padded[3]>=gpu_x,"spatial transform must bound FP32 cancellation");
  nodes.push_back(nodes.front());require(!index.reset(parts,nodes,error),"duplicate source nodes must fail");
  std::printf("scene_residency_probe passed=1 shared_parts=1 protected_actor=1 complete_admission=1 pending_preparation=1 mirrored_shear=1 hysteresis=1 authority_xz=1 exact_regions=1\n");
}
}
int main() {try {test();return 0;}catch(const std::exception& failure){std::fprintf(stderr,"scene_residency_probe failed=%s\n",failure.what());return 1;}}
