#include "PreloadedHalo.h"
#include "StreamNeighborhood.h"
#include "TerrainDensity.h"
#include "TerrainGeneration.h"
#include "TerrainVegetation.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace octaryn::client::world_presentation;
namespace {
using Neighbors=std::array<std::optional<SnapshotColumn>,8>;
constexpr std::array<std::pair<int,int>,8> directions{{
    {-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}};
constexpr OctarynServerTerrainMaterialRules materials{30,14,3,1,2,5,4};
struct Coverage { std::size_t cells{},leaves{},water{},edited{}; };
void require(bool condition,const char* message) {
  if(!condition)throw std::runtime_error(message);
}
std::size_t index(int x,int local_y,int z) {
  return static_cast<std::size_t>(x+32*(local_y+512*z));
}
Neighbors neighbors_for(int cx,int cz,bool edited) {
  Neighbors result;
  for(std::size_t i=0;i<directions.size();++i) {
    const auto [dx,dz]=directions[i];
    require(column_neighbor_index(dx,dz)==i,"halo neighbor indices must match signed spatial order");
    SnapshotColumn source{cx+dx,cz+dz,80+i,{}};
    source.authoritative_revision=900+i;
    if(edited) {
      const int x=cx*32+(dx<0?-1:dx>0?32:11);
      const int z=cz*32+(dz<0?-1:dz>0?32:17);
      source.edits={{x,-256,z,0},{x,-255,z,5},{x,-255,z,65535},
          {x,29,z,14},{x,30,z,21},{x,255,z,65535},
          {source.x*32+15,42,source.z*32+15,65535}};
    }
    result[i]=std::move(source);
  }
  return result;
}
Coverage compare_full_neighbors(StreamColumn& column,const Neighbors& neighbors) {
  const auto interior=column.blocks;
  const auto origin=column.origin;
  std::array<std::optional<StreamColumn>,8> full;
  for(std::size_t i=0;i<full.size();++i)
    if(neighbors[i])full[i]=generate_stream_column(*neighbors[i],column.epoch);
  prepare_stream_halo(column,neighbors);
  require(column.mesh_halo && column.mesh_halo->min_y==column.min_y &&
      column.mesh_halo->height==column.height,"prepared halo height must match its center");
  const auto& halo=*column.mesh_halo;
  require(halo.blocks.size()==132u*static_cast<std::size_t>(column.height) && halo.blocks.is_compact(),
      "halo must retain only the lossless compact border");
  require(column.blocks.storage_identity()==interior.storage_identity() && column.origin==origin,
      "halo preparation changed the center payload or exact origin");
  require(column.generated_blocks.storage_identity()==interior.storage_identity(),
      "generated payload witness must preserve the original center storage");
  for(std::size_t i=0;i<full.size();++i) {
    require(bool(halo.neighbors[i])==bool(neighbors[i]),"missing halo source acquired a false origin");
    if(neighbors[i]) {
      const ColumnOrigin expected{neighbors[i]->generator_revision,neighbors[i]->edits};
      require(*halo.neighbors[i]==expected && full[i]->origin && *full[i]->origin==expected,
          "halo identity must retain exact generator and ordered edits");
    }
  }
  Coverage coverage;
  std::vector<bool> visited(halo.blocks.size());
  for(int z=-1;z<=32;++z)for(int x=-1;x<=32;++x) {
    if(x!=-1 && x!=32 && z!=-1 && z!=32)continue;
    const int dx=x<0?-1:x>=32?1:0,dz=z<0?-1:z>=32?1:0;
    const auto found=std::find(directions.begin(),directions.end(),std::pair{dx,dz});
    require(found!=directions.end(),"border cell belongs to one of eight full neighbors");
    const auto neighbor=static_cast<std::size_t>(found-directions.begin());
    const int local_x=x-dx*32,local_z=z-dz*32;
    for(int y=0;y<column.height;++y) {
      const auto at=column_halo_index(x,y,z,column.height);
      require(at<visited.size() && !visited[at],"halo border index overlaps or escapes its compact storage");
      visited[at]=true;
      const auto expected=full[neighbor]?full[neighbor]->blocks[index(local_x,y,local_z)]:std::uint16_t{};
      if(halo.blocks[at]!=expected) {
        std::cerr<<"halo mismatch center="<<column.x<<','<<column.z<<" cell="<<x<<','<<y-256<<','<<z
            <<" expected="<<expected<<" actual="<<halo.blocks[at]<<'\n';
        require(false,"prepared border disagrees with independent full-column generation");
      }
      ++coverage.cells;
      coverage.leaves+=expected==7;coverage.water+=expected==14;coverage.edited+=expected==65535;
    }
  }
  require(std::all_of(visited.begin(),visited.end(),[](bool value){return value;}),
      "border comparison omitted a retained halo cell");
  return coverage;
}
struct Corner { int x{},z{},y{}; };
Corner canopy_corner() {
  using namespace octaryn::basegame::terrain;
  // Locate a deterministic canopy that crosses both column axes. Only fixture
  // selection uses vegetation rules; expected border values use full columns.
  for(int cz=-32;cz<=32;++cz)for(int cx=-32;cx<=32;++cx) {
    int leaf_y=-1000;
    emit_vegetation(cx*32,cz*32,materials,sample_column,[&](int x,int y,int z,std::uint16_t block) {
      if(block==LeavesBlock && x==cx*32-1 && z==cz*32-1 &&
          sample_block(sample_column(x,z),y,materials)==AirBlock)leaf_y=y;
    });
    if(leaf_y!=-1000)return {cx,cz,leaf_y};
  }
  throw std::runtime_error("deterministic diagonal canopy fixture was not found");
}
void validate_identity_and_copy_on_write(StreamColumn column,Neighbors neighbors) {
  const auto initial=column;
  require(column.origin && *column.origin==ColumnOrigin{3,{}},"generated exact origin missing");
  const auto& old_origin=*column.mesh_halo->neighbors[0];
  const auto expected_origin=old_origin;
  neighbors[0]->edits.push_back({neighbors[0]->x*32,255,neighbors[0]->z*32,4});
  require(old_origin==expected_origin,"prepared halo origin aliases mutable snapshot edits");
  ColumnOrigin changed=expected_origin;
  ++changed.generator_revision;
  require(changed!=expected_origin,"generator revision is absent from exact origin identity");
  changed=expected_origin;
  changed.edits.push_back({0,0,0,0});
  require(changed!=expected_origin,"edited air is absent from exact origin identity");
  const auto target=index(7,511,9);
  const auto old_block=static_cast<std::uint16_t>(column.blocks[target]);
  column.blocks[target]=old_block==65535?0:65535;
  require(column.blocks.storage_identity()!=column.generated_blocks.storage_identity() &&
      column.generated_blocks.storage_identity()==initial.blocks.storage_identity() &&
      initial.blocks[target]==old_block && column.generated_blocks[target]==old_block,
      "predicted/edit copy must detach while retaining the exact original witness");
  require(column.mesh_halo==initial.mesh_halo,"center COW edit mutated immutable private border");
  auto metadata=*neighbors[1];++metadata.revision;++metadata.authoritative_revision;
  const auto regenerated=generate_stream_column(metadata,initial.epoch+1);
  require(regenerated.origin && *regenerated.origin==*initial.mesh_halo->neighbors[1],
      "transport metadata must not change the exact generation origin");
}
}

void validate_preloaded_halo() {
  std::size_t checked{};
  for(const auto [cx,cz]:{std::pair{0,0},std::pair{-1,-1},std::pair{4,-3}}) {
    auto column=generate_stream_column({cx,cz,7,{}},42);
    const auto neighbors=neighbors_for(cx,cz,false);
    checked+=compare_full_neighbors(column,neighbors).cells;
    validate_identity_and_copy_on_write(column,neighbors);
  }
  const auto corner=canopy_corner();
  auto canopy=generate_stream_column({corner.x,corner.z,7,{}},42);
  checked+=compare_full_neighbors(canopy,neighbors_for(corner.x,corner.z,false)).cells;
  require(canopy.mesh_halo->blocks[column_halo_index(-1,corner.y+256,-1,512)]==7,
      "diagonal canopy crossing both axes was not exercised");
  auto edited=generate_stream_column({-7,-11,7,{{-224,-256,-352,0},{-193,255,-321,65535}}},42);
  auto neighbors=neighbors_for(edited.x,edited.z,true);
  const auto coverage=compare_full_neighbors(edited,neighbors);checked+=coverage.cells;
  require(coverage.water>=8 && coverage.edited>=16,"water and full uint16 border edits were not exercised");
  for(const auto [dx,dz]:directions) {
    const int x=dx<0?-1:dx>0?32:11,z=dz<0?-1:dz>0?32:17;
    for(const auto [y,block]:std::array<std::pair<int,std::uint16_t>,5>{{
        {-256,0},{-255,65535},{29,14},{30,21},{255,65535}}})
      require(edited.mesh_halo->blocks[column_halo_index(x,y+256,z,512)]==block,
          "ordered edited-air, water, water-level or endpoint override lost");
  }
  StreamSnapshot snapshot{42,1337,{}};
  for(const auto& neighbor:neighbors)snapshot.columns.push_back(*neighbor);
  StreamNeighborhood neighborhood;neighborhood.reset(snapshot,edited.x+1,edited.z+1,1);
  const SnapshotColumn center{edited.x,edited.z,7,{}};
  const auto clipped=neighborhood.capture(center);
  for(std::size_t i=0;i<directions.size();++i) {
    const auto [dx,dz]=directions[i];
    require(bool(clipped[i])==(dx>=0 && dz>=0),"requested window did not clip private halo neighbors");
  }
  checked+=compare_full_neighbors(edited,clipped).cells;
  checked+=compare_full_neighbors(edited,{}).cells;
  neighbors[0]->generator_revision=2;
  bool rejected=false;
  try { prepare_stream_halo(edited,neighbors); }
  catch(const std::invalid_argument&) { rejected=true; }
  require(rejected,"private halo accepted an unsupported neighbor generator");
  std::cout<<"preloaded_halo=passed border_cells="<<checked
      <<" directions=8 full_column_oracle=passed diagonal_canopy=passed edited_air=passed"
      <<" fluids=passed origin=passed cow_witness=passed missing_air=passed\n";
}

void validate_private_halo(const WorldStream& stream,const StreamColumn& column) {
  require(column.mesh_halo && column.origin &&
      column.generated_blocks.storage_identity()==column.blocks.storage_identity(),
      "real generation mailbox lacks private halo or original payload witness");
  std::size_t known{};
  for(std::size_t i=0;i<directions.size();++i) {
    const auto [dx,dz]=directions[i];
    if(!column.mesh_halo->neighbors[i])continue;
    ++known;
    std::uint16_t block{};
    require(!stream.try_block((column.x+dx)*32,255,(column.z+dz)*32,block),
        "private halo made an unpublished neighbor query-visible");
  }
  require(known>0,"real mailbox fixture did not prepare any known snapshot neighbor");
}

void validate_stream_request_activation(const std::filesystem::path& snapshot_path) {
  WorldStream stream(snapshot_path);
  StreamColumn ready;
  std::uint16_t block{};
  const auto idle_until=std::chrono::steady_clock::now()+std::chrono::milliseconds(150);
  do {
    require(stream.generated_columns()==0 && !stream.peek(ready) && !stream.poll(ready) &&
        !stream.try_block(0,255,0,block),"stream generated or published before its first explicit request");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  } while(std::chrono::steady_clock::now()<idle_until);
  require(stream.generated_columns()==0,"unrequested stream started generation during its idle interval");
  // These values equal StreamResidency's initial defaults. Activation cannot
  // depend on change_window reporting a spatial change.
  stream.request(0,0,2);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!stream.peek(ready)) {
    require(std::chrono::steady_clock::now()<deadline,"first request matching default window did not activate generation");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  require(ready.x==0 && ready.z==0 && stream.generated_columns()>0 &&
      !stream.try_block(0,255,0,block),"explicit activation lost center priority or published a ready query early");
  validate_private_halo(stream,ready);
  require(stream.publish(ready)==StreamPublication::Published &&
      stream.try_block(0,255,0,block) && block==5,"first explicit request failed exact query publication");
  std::cout<<"stream_first_request=passed idle_ms=150 default_window_activation=1 private_ready=1 exact_publication=1\n";
}
