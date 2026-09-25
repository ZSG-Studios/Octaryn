#include "PredictedColumn.h"
#include "StreamResidency.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <thread>

using namespace octaryn::client::world_presentation;
namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
template<class T> void write(std::ofstream& out,T value) {
  out.write(reinterpret_cast<const char*>(&value),sizeof(value));
}
void snapshot(const std::filesystem::path& path,std::uint64_t authority,std::uint16_t block,bool empty=false) {
  const auto temporary=path.string()+".tmp";
  std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
  out.write("OCSTRM01",8);write(out,3u);write(out,std::uint64_t{1});write(out,authority);
  write(out,0);write(out,0);write(out,1u);write(out,std::uint64_t{1337});
  write(out,0u);write(out,3u);write(out,std::uint64_t{});write(out,0u);write(out,0.0);write(out,0.0f);
  for(int index=0;index<8;++index)write(out,0.0f);
  write(out,0u);write(out,1u);write(out,1u);write(out,empty?0u:1u);
  write(out,0);write(out,0);write(out,0);write(out,0);write(out,0u);write(out,empty?0u:1u);
  if(!empty) {write(out,0);write(out,255);write(out,0);write(out,block);}
  out.close();check(bool(out),"write v3 authority baseline fixture");
  std::error_code ignored;std::filesystem::remove(path,ignored);
  std::filesystem::rename(temporary,path);
}
StreamColumn delivered(WorldStream& stream,std::uint64_t authority) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  StreamColumn column;
  while(std::chrono::steady_clock::now()<deadline) {
    if(stream.poll(column) && column.authoritative_revision==authority)return column;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  throw std::runtime_error("baseline authority delivery timeout: "+stream.status());
}
std::uint16_t block(const WorldStream& stream) {
  std::uint16_t value{};check(stream.try_block(0,255,0,value),"query delivered baseline");return value;
}
}
void validate_baseline_coverage(const std::filesystem::path& directory) {
  std::filesystem::create_directories(directory);
  const auto path=directory/"baseline.bin";
  snapshot(path,0,5);
  WorldStream stream(path);stream.request(0,0,1);
  auto source=delivered(stream,0);
  check(source.revision>1000000 && source.authoritative_revision==0,"separate huge content hash and zero world revision");
  check(stream.predict_block(1,0,255,0,0),"predict break");
  stream.resolve_block(1,true,1);
  check(block(stream)==0,"large stale hash must not cover accepted server revision one");
  snapshot(path,1,0);
  source=delivered(stream,1);
  check(block(stream)==0,"covering authority baseline preserves break");
  const auto hash=source.revision;
  const auto identity=source.blocks.storage_identity();
  check(stream.predict_block(2,0,255,0,5),"predict later edit");
  stream.resolve_block(2,true,3);
  snapshot(path,2,0);
  source=delivered(stream,2);
  check(source.revision==hash && source.blocks.storage_identity()==identity,"metadata-only advance reuses exact generation storage");
  check(block(stream)==5,"noncovering unrelated world revision preserves accepted overlay");
  snapshot(path,3,0);
  source=delivered(stream,3);
  check(source.revision==hash && source.blocks.storage_identity()==identity,"covering metadata still avoids regeneration");
  check(block(stream)==0,"covering unchanged-content authoritative baseline retires superseded accepted overlay");
  check(stream.predict_block(3,0,255,0,5),"predict unresolved command");
  snapshot(path,4,0);source=delivered(stream,4);
  check(block(stream)==5,"unrelated authoritative revision cannot retire unresolved command");
  stream.resolve_block(3,false,4);check(block(stream)==0,"unchanged-revision rejection restores exact baseline");
  check(stream.predict_block(4,0,255,0,5),"predict before empty baseline");
  stream.resolve_block(4,true,5);
  snapshot(path,5,0,true);source=delivered(stream,5);
  check(source.authoritative_revision==5 && block(stream)==0,"zero-edit full baseline carries authoritative coverage");

  StreamResidency residency;
  auto retained=std::make_shared<const StreamColumn>(source);
  check(residency.retain(source,retained),"bounded metadata mailbox admission");
  auto newer=source;newer.authoritative_revision=6;
  check(!StreamResidency::same_payload(source,newer),"metadata versions cannot alias delivery identity");
  check(residency.needs_publication(0,0,source.revision,6),"unchanged hash still schedules newer authority");
  std::puts("block_baseline_coverage PASS stale_large_hash metadata_only_storage_reuse covering_authority empty_baseline unresolved_retention");
}
