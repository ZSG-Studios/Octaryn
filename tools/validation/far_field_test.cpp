#include "FarFieldGenerator.h"
#include "StreamSnapshot.h"
#include "TerrainColumn.h"
#include <cstdio>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

using namespace octaryn::client::rendering;
namespace wp=octaryn::client::world_presentation;
namespace {
unsigned checks{};
void require(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
std::uint32_t features(std::uint16_t id,void*) {
  return id==14?2u:id==7?4u:id==9 || (id>=10 && id<=13)?8u:id>=22 && id<=28?16u:id?1u:0u;
}
FarFieldBuild generated(FarFieldKey key,std::span<const FarFieldEdit> edits={}) {
  FarFieldWork budget{5000,64,edits.size()};
  return far_field_generate(key,{key,17,edits,true},3,budget,features);
}
void boundaries() {
  require(far_field_key(-1,-4,-5,0)==FarFieldKey{-1,-1,-2,0},"negative floor division");
  require(far_field_key(INT32_MIN,0,INT32_MAX,2)==FarFieldKey{-33554432,0,33554431,2},"integer key limits");
  const auto key=far_field_key(0,248,0,0);
  FarFieldWork work{5000,64,32};
  require(far_field_generate(key,{key,1,{},false},3,work,features).state==FarFieldBuildState::Unknown,
      "missing edit coverage silently became sky");
  require(generated(key).node.state()==FarFieldState::Empty,"global vegetation bound failed");
  require(generated({INT32_MAX,0,0,2}).state==FarFieldBuildState::Unknown,"unsupported world coordinate became air");
  require(far_field_generate(key,{key,1,{},true},4,work,features).state==FarFieldBuildState::Unknown,"unsupported generator became air");
  require(generated(far_field_key(0,256,0,2)).node.state()==FarFieldState::Empty,"world upper bound not empty");
  work={0,0,0};
  const auto underground=far_field_key(0,-64,0,2);
  require(far_field_generate(underground,{underground,1,{},true},3,work,features).state==FarFieldBuildState::Deferred,
      "generation exceeded caller work budget");
  work={5000,64,0};
  require(far_field_generate(underground,{underground,1,{},true},3,work,features).state==FarFieldBuildState::Refine && work.voxel_samples==64,
      "height bound invented solid caves or sampled a full macro volume");
  const FarFieldEdit edits[]={{0,248,0,28},{0,248,0,0},{1,248,0,14},{2,248,0,7}};
  const auto edited=generated(key,edits);
  require(edited.node.complete() && edited.node.material[0]==0 && edited.node.material[1]==14 &&
      edited.node.material[2]==7 && edited.node.occupied==6 && edited.node.material_features==(2u|4u),
      "ordered authoritative air/fluid/foliage edits lost material parity");
  const FarFieldEdit outside[]={{4,248,0,28}};
  require(generated(key,outside).state==FarFieldBuildState::Unknown,"out-of-coverage edits accepted");
}
void invalidation() {
  FarFieldCache cache(4);cache.reset(100);
  const auto leaf=far_field_key(-1,248,-1,0),parent=far_field_key(-1,248,-1,1),root=far_field_key(-1,248,-1,2);
  const auto sibling=far_field_key(128,248,128,0);const auto empty=generated(leaf).node;
  const auto ticket=cache.ticket();
  for(auto key:{leaf,parent,root,sibling})require(cache.publish(key,empty,ticket),"cache publication failed");
  cache.invalidate(-1,248,-1);
  require(!cache.find(leaf) && !cache.find(parent) && !cache.find(root) && cache.find(sibling),"edit did not invalidate exactly affected ancestors");
  require(!cache.publish(root,empty,ticket),"in-flight stale summary resurrected after edit");
  cache.reset(101);require(cache.size()==0 && !cache.publish(leaf,empty,ticket),"world reset accepted stale work");
  for(int x=0;x<8;++x)require(cache.publish({x,62,0,0},empty,cache.ticket()),"bounded insertion failed");
  require(cache.size()==4 && !cache.find({0,62,0,0}),"cache capacity was not enforced");
  std::array<FarFieldNode,64> children;
  children.fill(empty);require(far_field_aggregate(children).state()==FarFieldState::Empty,"known children failed empty proof");
  children[0]={};require(far_field_aggregate(children).state()==FarFieldState::Unknown,"unknown child became empty ancestor");
  children[1].occupied=1;children[1].uniform&=~std::uint64_t{1};children[1].material[0]=28;
  const auto mixed=far_field_aggregate(children);
  require(mixed.state()==FarFieldState::Occupied && !mixed.complete() && !(mixed.uniform&2),"coarse witness invented complete uniform geometry");
  cache.invalidate_column(0,0);
  require(cache.size()==0,"column snapshot invalidation retained local descendants");
}
void parity() {
  std::set<std::uint16_t> witnessed;
  std::vector coordinates{std::pair{-1,-1},std::pair{0,0},std::pair{1,0}};
  bool ocean=false;
  for(int z=-2048;z<=2048 && !ocean;z+=128)for(int x=-2048;x<=2048 && !ocean;x+=128)
    if(octaryn::basegame::terrain::sample_column(x,z).terrain_height<28) {coordinates.emplace_back(x/32,z/32);ocean=true;}
  require(ocean,"could not locate generated water parity fixture");
  for(auto coordinate:coordinates) {
    wp::SnapshotColumn snapshot;snapshot.x=coordinate.first;snapshot.z=coordinate.second;snapshot.authoritative_revision=17;
    const auto source=wp::generate_stream_column(snapshot,100);
    std::set<FarFieldKey> keys;
    for(int y:{-256,-240,-160,-80,-16,0,28,60,120,240,248})
      keys.insert(far_field_key(snapshot.x*32,y,snapshot.z*32,0));
    std::set<std::uint16_t> found;
    source.blocks.visit_matching([](auto id){return id!=0;},[&](std::size_t index,std::uint16_t id) {
      if(!found.insert(id).second)return;
      const int x=int(index%32),y=int(index/32%512)-256,z=int(index/(32*512));
      keys.insert(far_field_key(snapshot.x*32+x,y,snapshot.z*32+z,0));witnessed.insert(id);
    });
    for(const auto key:keys) {
      const auto far=generated(key),resident=far_field_resident(key,source,features);
      require(far.state==FarFieldBuildState::Ready && resident.state==FarFieldBuildState::Ready,"parity leaf not ready");
      require(far.node.material==resident.node.material && far.node.occupied==resident.node.occupied &&
          far.node.material_features==resident.node.material_features,"generator leaf differs from actual complete StreamColumn");
    }
  }
  for(auto id:{6u,7u,9u,14u})require(witnessed.contains(std::uint16_t(id)),"parity fixture omitted trees, bushes, leaves or generated water");
  std::printf("far_field_parity material_ids=");for(auto id:witnessed)std::printf("%u,",unsigned(id));std::puts("");
}
}
int main() {
  try {boundaries();invalidation();parity();std::printf("far_field=passed checks=%u gpu=not_exercised\n",checks);return 0;}
  catch(const std::exception& e){std::fprintf(stderr,"far_field=failed checks=%u reason=%s\n",checks,e.what());return 1;}
}
