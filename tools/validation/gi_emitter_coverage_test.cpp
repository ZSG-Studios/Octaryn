#include "GICoverage.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace octaryn::client::rendering;
namespace {
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value){std::fprintf(stderr,"GI emitter coverage: %s\n",message);std::exit(1);}}
WorldLocalLight source(float x,float y,float z) {
  WorldLocalLight light;light.position_range={x,y,z,16};light.axis_v_type[3]=3;return light;
}
}
int main() {
  require(build_gi_emitter_cells({}).empty(),"empty selected list");
  std::vector<WorldLocalLight> lights{source(-.001f,-1,-1.001f),source(0,0,0),source(-.9f,-1,-1.9f)};
  auto cells=build_gi_emitter_cells(lights);
  require(cells==std::vector<GIEmitterCell>{{-1,-1,-2,0},{0,0,0,0}},"signed floor and duplicate identity");
  auto invalid=source(9,9,9);invalid.axis_v_type[3]=0;lights.push_back(invalid);
  invalid=source(10,10,10);invalid.color_intensity[3]=0;lights.push_back(invalid);
  invalid=source(11,11,11);invalid.color_intensity={0,0,0,1};lights.push_back(invalid);
  invalid=source(12,12,12);invalid.position_range[3]=0;lights.push_back(invalid);
  invalid=source(std::numeric_limits<float>::infinity(),0,0);lights.push_back(invalid);
  invalid=source(13,13,13);invalid.color_intensity[0]=std::numeric_limits<float>::quiet_NaN();lights.push_back(invalid);
  invalid=source(14,14,14);invalid.color_intensity[0]=-1;lights.push_back(invalid);
  require(build_gi_emitter_cells(lights)==cells,"inactive/non-voxel/invalid sources cannot suppress emission");
  lights={source(-1,0,0),source(-1.0001f,0,0),source(.9999f,0,0),source(1,0,0)};
  require(build_gi_emitter_cells(lights)==std::vector<GIEmitterCell>{{-2,0,0,0},{-1,0,0,0},{0,0,0,0},{1,0,0,0}},"cell boundary ownership");
  lights.clear();
  for(int i=32767;i>=-32768;--i)lights.push_back(source(float(i)+.5f,-2.3f,4.7f));
  cells=build_gi_emitter_cells(lights);require(cells.size()==65536,"full supported selection");
  for(int i=-32768;i<32768;++i)require(cells[std::size_t(i+32768)]==GIEmitterCell{i,-3,4,0},"full signed lexicographic ordering");
  require(!std::binary_search(cells.begin(),cells.end(),GIEmitterCell{32768,-3,4,0}),"omitted source remains emissive");
  lights.push_back(source(0,0,0));require(build_gi_emitter_cells(lights).empty(),"oversized invalid list preserves emission");
  std::printf("gi_emitter_coverage_cpu=passed checks=%u selected=65536 signed_cells=1 omitted=1 boundaries=1\n",checks);
}
