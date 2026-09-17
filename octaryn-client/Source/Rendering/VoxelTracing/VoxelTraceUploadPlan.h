#pragma once
#include "VoxelTraceWorld.h"

namespace octaryn::client::voxel_tracing {
struct UploadConfig {
  std::uint64_t max_gpu_bytes{512ull*1024*1024},max_upload_bytes{4ull*1024*1024};
  unsigned max_chunks{8192},max_chunk_uploads{32};
};
struct UploadLayout {
  unsigned capacity{},hash_count{};
  std::uint64_t frame_bytes{},total_bytes{},metadata_bytes{};
};
inline constexpr std::uint64_t ChunkDataBytes=LeafCount*8+MacroCount*8+MaterialWordCount*4;
// Identical uint arithmetic is required in Slang; negative coordinates cast modulo 2^32.
constexpr std::uint32_t trace_chunk_hash(ChunkKey key) {
  std::uint32_t h=std::uint32_t(key.x)*0x8da6b343u ^ std::uint32_t(key.y)*0xd8163841u ^ std::uint32_t(key.z)*0xcb1ab31fu;
  h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
}
struct UploadSnapshot {
  std::vector<std::shared_ptr<const TraceChunk>> slots;
  std::vector<ChunkHeader> headers;
  std::vector<std::uint32_t> hash;
  std::uint64_t world_epoch{},generation{};
};
struct UploadPlan {
  UploadSnapshot snapshot;
  std::vector<unsigned> uploads;
  unsigned pending{},capacity_unknown{},evicted{},invalidated{},max_hash_probes{};
  std::uint64_t upload_bytes{};
  bool metadata_changed{},journal_overflow{};
};
UploadLayout trace_upload_layout(const UploadConfig&);
UploadPlan plan_trace_upload(const VoxelTraceWorld&,const UploadSnapshot&,const UploadLayout&,const UploadConfig&,ChunkKey center);
}
