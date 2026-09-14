#include "Probe.h"
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <tuple>
namespace fluid_probe {
struct Grid {
  const FluidRules& rules;
  std::map<std::tuple<int32_t,int32_t,int32_t>,uint16_t> blocks;
  std::optional<BlockPosition> unavailable;
  int32_t floor=-1;
  size_t reads=0;
  explicit Grid(const FluidRules& r):rules(r) {}
  void set(BlockPosition p,uint16_t b) {blocks[{p.x,p.y,p.z}]=b;}
  bool read(BlockPosition p,uint16_t& b) {
    ++reads;
    if(p.y<WorldMinY || p.y>=WorldMaxYExclusive) throw std::runtime_error("reader received out-of-world Y");
    if(unavailable && p==*unavailable) return false;
    const auto it=blocks.find({p.x,p.y,p.z});
    b=it!=blocks.end()?it->second:p.y==floor?rules.stone:AirBlock;return true;
  }
  void expect(BlockPosition p,uint16_t wanted,const char* label) {
    uint16_t next=65535;
    require(evaluate_fluid(p,rules,[this](auto at,auto& b){return read(at,b);},next),std::string(label)+": unavailable");
    require(next==wanted,std::string(label)+": unexpected block "+std::to_string(next));
  }
  void defer(BlockPosition p,const char* label) {
    uint16_t next=65535;
    require(!evaluate_fluid(p,rules,[this](auto at,auto& b){return read(at,b);},next),label);
    require(next==65535,std::string(label)+": changed proposal on failure");
  }
};
void rules_cases(const FluidRules& r) {
  require(validate_fluid_rules(r),"valid module-defined rules including solid/replaceable leaves");
  auto bad=r;bad.lava[3]=r.water[7];require(!validate_fluid_rules(bad),"overlapping fluid tables rejected");
  bad=r;bad.water[1]=0;require(!validate_fluid_rules(bad),"air fluid rejected");
  bad=r;bad.replaceable.push_back(r.stone);require(!validate_fluid_rules(bad),"replaceable stone rejected");
  bad=r;bad.solid.push_back(r.water[0]);require(!validate_fluid_rules(bad),"solid fluid rejected");
  bad=r;bad.replaceable.push_back(r.replaceable[0]);require(!validate_fluid_rules(bad),"duplicate replaceable rejected");
}
void behavior(const FluidRules& r) {
  const BlockPosition p{-33,0,-65};
  for(const auto kind:{FluidKind::Water,FluidKind::Lava}) {
    for(int level=0;level<8;++level) {
      Grid g{r};g.set({p.x,1,p.z},r.make(kind,level));g.expect(p,r.make(kind,1),"falling every level resets to one");
      Grid side{r};side.set({p.x+1,0,p.z},r.make(kind,level));
      side.expect(p,level<7?r.make(kind,level+1):AirBlock,"flat spread level limit");
      if(level==0) {
        require(side.reads<4096,"flat donor bounded reads");
        std::cout<<"fluid_reads kind="<<(kind==FluidKind::Water?"water":"lava")<<" flat="<<side.reads<<'\n';
      }
      Grid drain{r};drain.set(p,r.make(kind,level));drain.expect(p,level==0?r.make(kind,0):AirBlock,"isolated source persists and flow drains");
    }
    Grid falling{r};falling.set({p.x+1,0,p.z},r.make(kind,0));falling.set({p.x+1,-1,p.z},AirBlock);falling.expect(p,AirBlock,"donor falls before sideways");
    falling.set({p.x+2,0,p.z},r.make(kind,0));falling.set({p.x+1,0,p.z+1},r.make(kind,0));falling.set({p.x+1,0,p.z-1},r.make(kind,0));
    falling.expect(p,r.make(kind,1),"three neighboring sources allow falling donor to spread sideways");
    Grid slope{r};slope.set({p.x+1,0,p.z},r.make(kind,0));slope.set({p.x+2,-1,p.z},AirBlock);slope.expect(p,AirBlock,"closer opposite drop suppresses flat side");
    slope.set({p.x,-1,p.z},AirBlock);slope.expect(p,r.make(kind,1),"equal immediate drops both spread");
    Grid depth{r};depth.set({p.x+1,0,p.z},r.make(kind,0));depth.set({p.x+5,-1,p.z},AirBlock);
    depth.expect(p,kind==FluidKind::Water?AirBlock:r.make(kind,1),"water four-step slope versus lava two-step slope");
    Grid vegetation{r};vegetation.set(p,r.replaceable[0]);vegetation.expect(p,r.replaceable[0],"dry vegetation remains");
    vegetation.set({p.x,1,p.z},r.make(kind,3));vegetation.expect(p,r.make(kind,1),"falling replaces vegetation");
    Grid lateral{r};lateral.set(p,r.replaceable[0]);lateral.set({p.x+1,0,p.z},r.make(kind,0));lateral.set({p.x-1,0,p.z},r.make(kind,0));
    lateral.expect(p,r.make(kind,1),"vegetation direct contact precedes source creation");
    Grid source{r};source.set({p.x+1,0,p.z},r.make(kind,0));source.set({p.x-1,0,p.z},r.make(kind,0));
    source.expect(p,r.make(kind,kind==FluidKind::Water?0:1),"only water creates supported sources");
    source.set({p.x,-1,p.z},r.water[0]);source.expect(p,r.make(kind,kind==FluidKind::Water?0:1),"water-source support");
    source.set({p.x,-1,p.z},r.water[3]);source.expect(p,r.make(kind,1),"flowing water cannot support new source");
    Grid bottom{r};bottom.floor=WorldMinY-1;bottom.set({-33,WorldMinY,-64},r.make(kind,0));bottom.expect({-33,WorldMinY,-65},r.make(kind,1),"bottom donors cannot fall out of world");
    Grid top{r};top.floor=WorldMaxYExclusive-2;top.set({p.x,WorldMaxYExclusive-1,p.z},r.make(kind,3));top.expect({p.x,WorldMaxYExclusive-1,p.z},AirBlock,"top has known empty above");
  }
  for(const BlockPosition d: {BlockPosition{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}) {
    Grid g{r};g.set(p,r.lava[0]);g.set({p.x+d.x,p.y+d.y,p.z+d.z},r.water[7]);g.expect(p,r.stone,"lava source hardens on all six water contacts");
  }
  Grid water{r};water.set(p,r.water[0]);water.set({p.x,1,p.z},r.lava[0]);water.expect(p,r.water[0],"water source survives lava");
  Grid conflict{r};conflict.set({p.x+1,0,p.z},r.water[0]);conflict.set({p.x-1,0,p.z},r.lava[0]);conflict.expect(p,AirBlock,"ambiguous incoming kinds leave air");
  Grid solid{r};solid.set(p,r.stone);solid.unavailable=BlockPosition{p.x,1,p.z};solid.expect(p,r.stone,"unaffected solid needs no neighbors");
  for(const BlockPosition missing:{p,BlockPosition{p.x,1,p.z},BlockPosition{p.x+1,0,p.z},BlockPosition{p.x+2,-1,p.z}}) {
    Grid g{r};g.set({p.x+1,0,p.z},r.water[0]);g.unavailable=missing;g.defer(p,"required unknown sample defers");
  }
  for(const auto edge:{std::numeric_limits<int32_t>::min(),std::numeric_limits<int32_t>::max()}) {
    Grid g{r};g.defer({edge,0,0},"X overflow defers");g.defer({0,0,edge},"Z overflow defers");
  }
  Grid invalid{r};invalid.defer({0,WorldMinY-1,0},"below world invalid");invalid.defer({0,WorldMaxYExclusive,0},"above world invalid");require(invalid.reads==0,"invalid Y does not sample");
}
}
int main() {
  using namespace fluid_probe;
  try {
    const FluidRules sequential{{14,15,16,17,18,19,20,21},{22,23,24,25,26,27,28,29},5,{6,7},{5,7}};
    const FluidRules remapped{{601,17,5000,31,99,203,65500,301},{902,76,411,65,802,42,3000,1001},123,{77,88},{123,88}};
    for(const auto& r:{sequential,remapped}) {rules_cases(r);behavior(r);check_apply(r);}
    check_scheduler(remapped);
    check_fluid_abi(remapped);
    std::cout<<"server_fluid_probe PASS checks="<<checks<<" evaluator=passed scheduler=passed cpu_fixture=1\n";return 0;
  } catch(const std::exception& e) {std::cerr<<"server_fluid_probe FAIL "<<e.what()<<'\n';return 1;}
}
