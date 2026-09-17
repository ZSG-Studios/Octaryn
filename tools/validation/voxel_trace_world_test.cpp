#include "VoxelTraceWorld.h"
#include "VoxelTraceUploadPlan.h"
#include "../../octaryn-client/Source/WorldPresentation/WorldStream/PredictedColumn.h"
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::voxel_tracing;
namespace wp=octaryn::client::world_presentation;
namespace {
unsigned checks{};
void require(bool condition,const char* message) {
  ++checks;if(!condition)throw std::runtime_error(message);
}
wp::StreamColumn column(int x,int z,int minimum,int height) {
  wp::StreamColumn c;c.x=x;c.z=z;c.min_y=minimum;c.height=height;
  c.blocks.resize(std::size_t(height)*1024);return c;
}
std::size_t index(const wp::StreamColumn& c,unsigned x,int y,unsigned z) {
  return x+32*(std::size_t(y-c.min_y)+std::size_t(c.height)*z);
}
void hierarchy() {
  auto c=column(-1,-2,-35,70);
  for(unsigned z=0;z<32;++z)for(int y=-35;y<35;++y)for(unsigned x=0;x<32;++x) {
    const auto hash=(x*131u+unsigned(y+35)*71u+z*17u);
    c.blocks[index(c,x,y,z)]=hash%7==0?std::uint16_t(hash%65535+1):0;
  }
  c.blocks.compact();const auto storage=c.blocks.storage_identity();
  require(!build_trace_chunk(std::make_shared<const wp::StreamColumn>(c),40),"nonintersecting worker chunk accepted");
  VoxelTraceWorld world;require(world.publish(c).changed_chunks==4,"unaligned signed column chunk count");
  require(c.blocks.storage_identity()==storage && c.blocks.is_compact(),"hierarchy expanded or rewrote published blocks");
  for(const auto& [key,chunk]:world.chunks()) {
    ChunkPayload payload;chunk->export_payload(payload);
    require(payload.header.coordinate==std::array<int,3>{key.x,key.y,key.z},"GPU global chunk identity");
    std::array<std::uint64_t,512> leaves{};std::array<std::uint64_t,8> macros{};unsigned top=0;
    for(unsigned z=0;z<32;++z)for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
      const int wy=key.y*32+int(y);const bool known=wy>=-35 && wy<35;
      const std::uint16_t expected=known?c.blocks[index(c,x,wy,z)]:0;
      const auto value=world.sample(-32+int(x),wy,-64+int(z));
      require(value.residency==(known?(expected?Residency::Occupied:Residency::Air):Residency::Unknown),"known air versus nonresident distinction");
      require(value.block==expected,"exact compact material reconstruction");
      const auto linear=x+32*(y+32*z);
      require(((payload.materials[linear/2]>>((linear&1)*16))&65535)==expected,"GPU material packing");
      if(expected) {
        const unsigned leaf=x/4+8*(y/4+8*(z/4)),bit=x%4+4*(y%4+4*(z%4));
        leaves[leaf]|=1ull<<bit;
        const unsigned macro=x/16+2*(y/16+2*(z/16)),child=(x/4)%4+4*((y/4)%4+4*((z/4)%4));
        macros[macro]|=1ull<<child;top|=1u<<macro;
      }
    }
    require(payload.leaves==leaves && payload.macros==macros && payload.header.macro_mask==top,"leaf/macro occupancy differs from scalar voxels");
  }
  require(world.sample(0,0,-64).residency==Residency::Unknown,"unpublished neighboring chunk became sky");
}
void lifecycle() {
  VoxelTraceWorld world({2,2});auto c=column(0,0,0,32);c.blocks[index(c,31,31,31)]=5;
  require(world.publish(c).status==PublishStatus::Published,"initial publication");
  const auto held=world.find({0,0,0});const auto epoch=held->geometry_epoch;
  require(held->leaves[511]==(1ull<<63) && held->macros[7]==(1ull<<63) && held->macro_mask==128,"highest mask bits");
  auto same=c;same.authoritative_revision=99;
  require(world.publish(same).status==PublishStatus::Unchanged && world.epoch()==epoch,"metadata revision changed geometry");
  c.blocks[index(c,31,31,31)]=6;
  require(world.publish(c).changed_chunks==1 && world.epoch()>epoch,"material-only edit kept stale epoch");
  require(held->sample(31,31,31).block==5,"edit mutated retained immutable upload snapshot");
  c.blocks[index(c,31,31,31)]=0;world.publish(c);
  require(world.find({0,0,0})->macro_mask==0 && world.sample(31,31,31).residency==Residency::Air,"edited air failed to clear parents");
  std::vector<TraceChange> changes;
  require(!world.changes_since(0,changes),"change overflow silently accepted stale cursor");
  const auto before=world.epoch();
  auto too_large=column(1,0,0,96);
  require(world.publish(too_large).status==PublishStatus::Capacity && world.chunks().size()==1 && world.epoch()==before,"capacity rejection partially published a column");
  require(world.remove(0,0)==1 && world.sample(31,31,31).residency==Residency::Unknown,"eviction remained known air");
  world.publish(c);require(world.find({0,0,0})->geometry_epoch>before,"reload reused evicted cache identity");
  require(world.changes_since(world.epoch(),changes) && changes.empty(),"current epoch returned spurious work");
}
void predictions() {
  auto base=column(-1,0,0,32);base.blocks[index(base,31,0,0)]=5;base.blocks.compact();
  VoxelTraceWorld world;world.publish(base);wp::PredictedBlocks edits;
  require(edits.add(1,-1,0,0,0),"prediction add");world.publish(wp::compose_predicted_column(base,edits));
  require(world.sample(-1,0,0).residency==Residency::Air && base.blocks[index(base,31,0,0)]==5,"predicted air changed authority or missed trace world");
  edits.resolve(1,false,0);world.publish(wp::compose_predicted_column(base,edits));
  require(world.sample(-1,0,0).block==5,"rejected prediction did not restore trace occupancy");
  edits.add(2,-1,0,0,28);edits.resolve(2,true,42);edits.cover(-1,0,41);
  world.publish(wp::compose_predicted_column(base,edits));const auto epoch=world.epoch();
  require(world.sample(-1,0,0).block==28,"acknowledged prediction retired ahead of authority");
  base.blocks[index(base,31,0,0)]=28;base.authoritative_revision=42;edits.cover(-1,0,42);
  world.publish(wp::compose_predicted_column(base,edits));
  require(edits.edits().empty() && world.epoch()==epoch,"authoritative cover churned identical geometry");
}
void precision() {
  const ChunkKey key{134217728,-67108864,-134217728};
  const std::array<std::int64_t,3> anchor{4294967297ll,-2147483647ll,-4294967295ll};
  std::array<float,3> relative{};
  require(relative_chunk_origin(key,anchor,relative) && relative==std::array<float,3>{-1,-1,-1},"rebasing rounded absolute floats before subtraction");
  require(!relative_chunk_origin(key,{0,0,0},relative),"distant non-exact coordinate accepted");
  require(floor_div(-1,32)==-1 && floor_div(-32,32)==-1 && floor_div(-33,32)==-2,"negative exact boundaries");
}
void preparation() {
  VoxelTraceWorld world;auto c=column(0,0,0,32);world.publish(c);
  c.blocks[0]=5;auto old=world.prepare_input(c);
  c.blocks[0]=6;auto current=prepare_trace_column(world.prepare_input(c));
  require(world.publish_prepared(std::move(current)).status==PublishStatus::Published,"prepared publication failed");
  require(world.publish_prepared(prepare_trace_column(std::move(old))).status==PublishStatus::Stale && world.sample(0,0,0).block==6,
      "late worker result overwrote newer predicted geometry");
  auto independent=world.prepare_input(c);world.publish(column(1,0,0,32));
  require(world.publish_prepared(prepare_trace_column(std::move(independent))).status==PublishStatus::Unchanged,
      "unrelated column publication invalidated worker input");
  auto evicted=world.prepare_input(c);world.remove(0,0);world.publish(c);
  require(world.publish_prepared(prepare_trace_column(std::move(evicted))).status==PublishStatus::Stale,
      "eviction/reload accepted an obsolete source identity");
}
bool hash_contains(const UploadSnapshot& snapshot,ChunkKey key) {
  unsigned at=trace_chunk_hash(key)&unsigned(snapshot.hash.size()-1);
  for(unsigned probe=0;probe<snapshot.hash.size();++probe) {
    const auto entry=snapshot.hash[at];if(!entry)return false;
    if(snapshot.headers[entry-1].coordinate==std::array<int,3>{key.x,key.y,key.z})return true;
    at=(at+1)&unsigned(snapshot.hash.size()-1);
  }
  return false;
}
void upload_plans() {
  UploadConfig config;config.max_gpu_bytes=1024*1024;config.max_upload_bytes=70000;
  config.max_chunks=2;config.max_chunk_uploads=1;
  const auto layout=trace_upload_layout(config);
  require(layout.capacity==2 && layout.total_bytes<=config.max_gpu_bytes,"GPU byte budget ignored two frame slots");
  VoxelTraceWorld world({16,2});for(int x:{0,10,20,30,40})world.publish(column(x,0,0,32));
  const auto first=plan_trace_upload(world,{},layout,config,{0,0,0});
  require(first.journal_overflow && first.uploads.size()==1 && first.pending==1 && first.capacity_unknown==3,
      "overflow resync or bounded initial upload failed");
  require(first.upload_bytes<=config.max_upload_bytes && hash_contains(first.snapshot,{0,0,0}),"nearest upload priority or byte limit failed");
  const auto second=plan_trace_upload(world,first.snapshot,layout,config,{0,0,0});
  const auto idle=plan_trace_upload(world,second.snapshot,layout,config,{0,0,0});
  require(idle.upload_bytes==0 && idle.uploads.empty() && !idle.metadata_changed,"unchanged scene rewrote GPU resources");
  auto moved=plan_trace_upload(world,second.snapshot,layout,config,{20,0,0});
  require(moved.evicted==1 && moved.capacity_unknown==3 && hash_contains(moved.snapshot,{20,0,0}) &&
      !hash_contains(moved.snapshot,{0,0,0}),"capacity eviction did not follow camera priority");
  require(hash_contains(second.snapshot,{0,0,0}),"new frame mutated prior in-flight snapshot");
  world.remove(20,0);const auto removed=plan_trace_upload(world,moved.snapshot,layout,config,{20,0,0});
  require(!hash_contains(removed.snapshot,{20,0,0}),"deleted geometry remained in current lookup");
  auto changed=column(10,0,0,32);changed.blocks[0]=28;world.publish(changed);
  auto edited=plan_trace_upload(world,removed.snapshot,layout,config,{20,0,0});
  require(edited.invalidated==1 && edited.upload_bytes<=config.max_upload_bytes,"material edit ignored epoch or upload bound");
  UploadConfig collisions=config;collisions.max_chunks=4;collisions.max_chunk_uploads=4;collisions.max_upload_bytes=300000;
  const auto collision_layout=trace_upload_layout(collisions);VoxelTraceWorld collision_world;
  std::vector<int> keys;const unsigned bucket=trace_chunk_hash({-1,0,0})&(collision_layout.hash_count-1);
  for(int x=-1000;keys.size()<3;++x)if((trace_chunk_hash({x,0,0})&(collision_layout.hash_count-1))==bucket) {
    keys.push_back(x);collision_world.publish(column(x,0,0,32));
  }
  const auto collision=plan_trace_upload(collision_world,{},collision_layout,collisions,{0,0,0});
  for(int x:keys)require(hash_contains(collision.snapshot,{x,0,0}),"full-key collision lookup lost a negative chunk");
  require(collision.max_hash_probes>=3 && !hash_contains(collision.snapshot,{999,0,0}),"collision chain or absent key lookup failed");
}
}
int main() {
  try {hierarchy();lifecycle();predictions();precision();preparation();upload_plans();}
  catch(const std::exception& error) {std::fprintf(stderr,"voxel_trace_world=failed reason=%s checks=%u\n",error.what(),checks);return 1;}
  std::printf("voxel_trace_world=passed checks=%u compact_source=1 exact_masks=1 packed_materials=1 epochs=1 prediction_ack_order=1 bounded_residency=1 negative_rebase=1 stale_worker_rejection=1 bounded_upload=1 immutable_frame_slots=1 hash_collisions=1\n",checks);
}
