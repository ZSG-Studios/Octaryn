#include "BlockLightSelection.h"
#include <cstdio>
#include <cstdlib>
#include <numeric>

using namespace octaryn::client::rendering;
namespace {
unsigned checks{};
void require(bool value,const char* message) {
  ++checks;if(!value) {std::fprintf(stderr,"block light selection: %s\n",message);std::exit(1);}
}
WorldLocalLight light(float x,float y,float z,float range,unsigned identity) {
  WorldLocalLight result;result.position_range={x,y,z,range};
  result.color_intensity={float(identity),.5f,.25f,1};result.axis_v_type[3]=3;return result;
}
std::vector<WorldLocalLight> oracle(const std::vector<WorldLocalLight>& blocks,
    const std::vector<WorldLocalLight>& explicit_lights,std::array<float,3> center) {
  struct Entry {float influence,distance;const WorldLocalLight* light;};
  const auto entry=[&](const WorldLocalLight& source) {
    float distance{};
    for(unsigned axis=0;axis<3;++axis) {
      const float delta=source.position_range[axis]-center[axis];distance+=delta*delta;
    }
    return Entry{std::max(0.f,std::sqrt(distance)-source.position_range[3]-8),distance,&source};
  };
  const auto before=[](const Entry& a,const Entry& b) {
    return a.influence==b.influence?a.distance<b.distance:a.influence<b.influence;
  };
  std::vector<Entry> all;
  for(const auto& source:blocks)all.push_back(entry(source));
  std::stable_sort(all.begin(),all.end(),before);
  all.resize(std::min(all.size(),std::min<std::size_t>(4096,65536-explicit_lights.size())));
  for(const auto& source:explicit_lights)all.push_back(entry(source));
  std::stable_sort(all.begin(),all.end(),before);
  std::vector<WorldLocalLight> result;
  for(const auto& value:all)result.push_back(*value.light);
  return result;
}
std::vector<WorldLocalLight> verify(const std::vector<WorldLocalLight>& blocks,
    const std::vector<WorldLocalLight>& explicit_lights={},std::array<float,3> center={}) {
  const auto budget=std::min<std::size_t>(4096,65536-explicit_lights.size());
  BlockLightSelection selection(center,explicit_lights.size());
  for(const auto& source:blocks) {
    selection.add(source);require(selection.selected_count()<=budget,"candidate storage exceeds fixed block budget");
  }
  require(selection.selected_count()==std::min(budget,blocks.size()),"all resident emitters are eligible");
  auto actual=selection.finish(explicit_lights);const auto expected=oracle(blocks,explicit_lights,center);
  require(actual.size()==expected.size() && actual.size()<=65536,"bounded final selected list");
  for(std::size_t index=0;index<actual.size();++index)
    require(world_local_light_equal(actual[index],expected[index]),"full stable-sort oracle mismatch");
  return actual;
}
}
int main() {
  verify({});verify({light(100000,0,0,1,1)});
  std::vector<WorldLocalLight> distant;
  for(unsigned i=0;i<81;++i)distant.push_back(light(10000+float(i)*32,0,0,1,i));
  require(verify(distant).size()==distant.size(),"distant residents must survive fog-off's former64m cutoff");
  std::vector<WorldLocalLight> ties;
  for(unsigned i=0;i<5000;++i)ties.push_back(light(100,0,0,16,i));
  auto selected=verify(ties);
  for(unsigned i=0;i<4096;++i)require(selected[i].color_intensity[0]==float(i),"stable source-order tie selection");
  std::vector<WorldLocalLight> explicit_lights;
  for(unsigned i=0;i<65535;++i) {
    auto value=light(100,0,0,16,100000+i);value.axis_v_type[3]=float(i%3);explicit_lights.push_back(value);
  }
  const std::vector<WorldLocalLight> broad{light(30,0,0,1,1),light(5000,0,0,6000,2)};
  selected=verify(broad,explicit_lights);
  require(selected.front().color_intensity[0]==2,"distant broad influence must outrank nearby narrow source");
  selected=verify(ties,explicit_lights);
  require(selected.front().color_intensity[0]==0,"block source precedes explicit source on exact tie");
  explicit_lights.push_back(light(0,0,0,4,999999));verify(ties,explicit_lights);
  std::vector<WorldLocalLight> population;population.reserve(200003);
  std::uint32_t random=0x67432;
  const auto next=[&] {random=random*1664525u+1013904223u;return random;};
  for(unsigned i=0;i<200003;++i) {
    const float x=float(int(next()%20001)-10000),y=float(int(next()%1025)-512),z=float(int(next()%20001)-10000);
    population.push_back(light(x,y,z,float(1+next()%8192),i));
  }
  verify(population,{}, {-132,44,980});
  explicit_lights.resize(65504);verify(population,explicit_lights,{-4096,-256,-4096});
  std::puts("block_light_selection=passed full_sort_oracle=1 bounded_candidates=4096 max_sources=200003 "
      "stable_ties=1 distant_residents=1 distant_broad=1 explicit_identity=1 budget_saturation=1 fog_input=none");
  std::printf("checks=%u\n",checks);
}
