#include "VoxelTraceWorld.h"
#include <algorithm>
#include <limits>

namespace octaryn::client::voxel_tracing {
VoxelSample TraceChunk::sample(unsigned x,unsigned y,unsigned z) const {
  if(x>=32 || z>=32 || y<min_local_y || y>=max_local_y)return {};
  const auto local_y=std::int64_t(key.y)*32+y-source->min_y;
  const auto index=std::size_t(x)+32*(std::size_t(local_y)+std::size_t(source->height)*z);
  const auto block=source->blocks[index];
  return {block?Residency::Occupied:Residency::Air,block,geometry_epoch};
}
std::shared_ptr<TraceChunk> build_trace_chunk(
    std::shared_ptr<const world_presentation::StreamColumn> source,std::int32_t chunk_y) {
  const auto origin=std::int64_t(chunk_y)*32;
  if(!source || source->height<=0 || std::uint64_t(source->height)*1024!=source->blocks.size() ||
      origin>=std::int64_t(source->min_y)+source->height || origin+32<=source->min_y)return {};
  auto chunk=std::make_shared<TraceChunk>();
  chunk->key={source->x,chunk_y,source->z};chunk->source=std::move(source);
  chunk->min_local_y=unsigned(std::max(std::int64_t(chunk->source->min_y)-origin,std::int64_t(0)));
  chunk->max_local_y=unsigned(std::min(std::int64_t(chunk->source->min_y)+chunk->source->height-origin,std::int64_t(32)));
  std::shared_ptr<std::array<std::uint32_t,MaterialWordCount>> materials;
  std::array<std::uint32_t,32> row{};
  for(unsigned z=0;z<32;++z)for(unsigned y=chunk->min_local_y;y<chunk->max_local_y;++y) {
    const auto local_y=origin+y-chunk->source->min_y;
    const auto first=32*(std::size_t(local_y)+std::size_t(chunk->source->height)*z);
    chunk->source->blocks.read_range(first,row);
    for(unsigned x=0;x<32;++x)if(row[x]) {
      if(!materials)materials=std::make_shared<std::array<std::uint32_t,MaterialWordCount>>();
      const auto index=voxel_index(x,y,z);
      (*materials)[index/2]|=row[x]<<((index%2)*16);
      chunk->leaves[leaf_index(x,y,z)]|=std::uint64_t(1)<<leaf_bit(x,y,z);
      chunk->macros[macro_index(x,y,z)]|=std::uint64_t(1)<<macro_bit(x,y,z);
      chunk->macro_mask|=1u<<macro_index(x,y,z);
    }
  }
  chunk->packed_materials=std::move(materials);
  return chunk;
}
std::span<const std::uint32_t> TraceChunk::material_words() const {
  static constexpr std::array<std::uint32_t,MaterialWordCount> empty{};
  return packed_materials?std::span<const std::uint32_t>(*packed_materials):std::span<const std::uint32_t>(empty);
}
bool equal_trace_content(const TraceChunk& a,const TraceChunk& b) {
  if(a.min_local_y!=b.min_local_y || a.max_local_y!=b.max_local_y || a.leaves!=b.leaves)return false;
  // Non-air material changes invalidate lighting even when occupancy is identical.
  const auto left=a.material_words(),right=b.material_words();
  return std::equal(left.begin(),left.end(),right.begin());
}
void TraceChunk::export_payload(ChunkPayload& out) const {
  out.header={};out.header.coordinate={key.x,key.y,key.z};out.header.macro_mask=macro_mask;
  out.header.geometry_epoch=geometry_epoch;out.header.min_local_y=min_local_y;
  out.header.max_local_y=max_local_y;out.header.flags=ChunkKnown;
  out.leaves=leaves;out.macros=macros;
  const auto words=material_words();std::copy(words.begin(),words.end(),out.materials.begin());
}
bool relative_chunk_origin(ChunkKey key,const std::array<std::int64_t,3>& anchor,std::array<float,3>& result) {
  const std::array<std::int64_t,3> origin{std::int64_t(key.x)*32,std::int64_t(key.y)*32,std::int64_t(key.z)*32};
  for(unsigned axis=0;axis<3;++axis) {
    // Compare before subtraction so adversarial anchors cannot overflow int64.
    if(anchor[axis]<origin[axis]-(1ll<<24) || anchor[axis]>origin[axis]+(1ll<<24))return false;
  }
  for(unsigned axis=0;axis<3;++axis)result[axis]=float(origin[axis]-anchor[axis]);
  return true;
}
}
