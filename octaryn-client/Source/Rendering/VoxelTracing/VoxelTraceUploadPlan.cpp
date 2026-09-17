#include "VoxelTraceUploadPlan.h"
#include <algorithm>
#include <limits>
#include <set>

namespace octaryn::client::voxel_tracing {
UploadLayout trace_upload_layout(const UploadConfig& config) {
  UploadLayout layout;
  const auto limit=std::numeric_limits<std::uint32_t>::max()/MaterialWordCount;
  auto capacity=unsigned(std::min({std::uint64_t(config.max_chunks),std::uint64_t(limit),
    config.max_gpu_bytes/(2*(ChunkDataBytes+sizeof(ChunkHeader)))}));
  while(capacity) {
    unsigned hash=2;while(hash<capacity*2)hash*=2;
    const auto metadata=std::uint64_t(capacity)*sizeof(ChunkHeader)+std::uint64_t(hash)*4;
    const auto frame=std::uint64_t(capacity)*ChunkDataBytes+metadata;
    if(frame<=config.max_gpu_bytes/2 && metadata+ChunkDataBytes<=config.max_upload_bytes && config.max_chunk_uploads)
      return {capacity,hash,frame,frame*2,metadata};
    --capacity;
  }
  return layout;
}
namespace {
std::uint64_t distance(ChunkKey a,ChunkKey b) {
  const auto delta=[](int x,int y){const auto d=std::int64_t(x)-y;return std::uint64_t(d<0?-d:d);};
  return std::max({delta(a.x,b.x),delta(a.y,b.y),delta(a.z,b.z)});
}
ChunkHeader header(const TraceChunk& chunk,unsigned slot) {
  ChunkHeader value{};value.coordinate={chunk.key.x,chunk.key.y,chunk.key.z};
  value.macro_mask=chunk.macro_mask;value.geometry_epoch=chunk.geometry_epoch;
  value.leaf_offset=slot*LeafCount;value.macro_offset=slot*MacroCount;value.material_offset=slot*MaterialWordCount;
  value.min_local_y=chunk.min_local_y;value.max_local_y=chunk.max_local_y;value.flags=ChunkKnown;
  return value;
}
}
UploadPlan plan_trace_upload(const VoxelTraceWorld& world,const UploadSnapshot& previous,
    const UploadLayout& layout,const UploadConfig& config,ChunkKey center) {
  UploadPlan plan;if(!layout.capacity)return plan;
  plan.snapshot.slots=previous.slots;plan.snapshot.slots.resize(layout.capacity);
  std::vector<TraceChange> changes;plan.journal_overflow=!world.changes_since(previous.world_epoch,changes);
  std::vector<std::shared_ptr<const TraceChunk>> wanted;
  wanted.reserve(world.chunks().size());for(const auto& [key,chunk]:world.chunks())wanted.push_back(chunk);
  const auto nearest=[&](const auto& a,const auto& b) {
    const auto da=distance(a->key,center),db=distance(b->key,center);return da==db?a->key<b->key:da<db;
  };
  const auto count=std::min<std::size_t>(layout.capacity,wanted.size());
  std::partial_sort(wanted.begin(),wanted.begin()+std::ptrdiff_t(count),wanted.end(),nearest);
  plan.capacity_unknown=unsigned(wanted.size()-count);wanted.resize(count);
  std::map<ChunkKey,std::shared_ptr<const TraceChunk>> desired;
  for(const auto& chunk:wanted)desired.emplace(chunk->key,chunk);
  std::set<ChunkKey> resident;std::vector<unsigned> free;
  for(unsigned slot=0;slot<layout.capacity;++slot) {
    auto& chunk=plan.snapshot.slots[slot];
    if(chunk) {
      const auto current=desired.find(chunk->key);
      if(current==desired.end()) {chunk.reset();++plan.evicted;}
      else if(current->second->geometry_epoch!=chunk->geometry_epoch) {chunk.reset();++plan.invalidated;}
      else {chunk=current->second;resident.insert(chunk->key);}
    }
    if(!chunk)free.push_back(slot);
  }
  const auto jobs=unsigned(std::min<std::uint64_t>(config.max_chunk_uploads,
    (config.max_upload_bytes-layout.metadata_bytes)/ChunkDataBytes));
  for(const auto& chunk:wanted)if(!resident.contains(chunk->key)) {
    if(plan.uploads.size()<jobs && !free.empty()) {
      const unsigned slot=free.back();free.pop_back();plan.snapshot.slots[slot]=chunk;
      plan.uploads.push_back(slot);
    } else ++plan.pending;
  }
  for(unsigned slot=0;slot<layout.capacity;++slot)if(plan.snapshot.slots[slot])
    plan.snapshot.headers.push_back(header(*plan.snapshot.slots[slot],slot));
  std::sort(plan.snapshot.headers.begin(),plan.snapshot.headers.end(),[](const auto& a,const auto& b){return a.coordinate<b.coordinate;});
  plan.snapshot.hash.resize(layout.hash_count);
  for(unsigned index=0;index<plan.snapshot.headers.size();++index) {
    const auto& key=plan.snapshot.headers[index].coordinate;
    unsigned at=trace_chunk_hash({key[0],key[1],key[2]})&(layout.hash_count-1),probes=1;
    while(plan.snapshot.hash[at]) {at=(at+1)&(layout.hash_count-1);++probes;}
    plan.snapshot.hash[at]=index+1;plan.max_hash_probes=std::max(plan.max_hash_probes,probes);
  }
  plan.metadata_changed=plan.snapshot.headers!=previous.headers || plan.snapshot.hash!=previous.hash;
  plan.upload_bytes=plan.uploads.size()*ChunkDataBytes+(plan.metadata_changed?
    plan.snapshot.headers.size()*sizeof(ChunkHeader)+plan.snapshot.hash.size()*4:0);
  plan.snapshot.world_epoch=world.epoch();
  plan.snapshot.generation=previous.generation+(plan.metadata_changed?1:0);
  return plan;
}
}
