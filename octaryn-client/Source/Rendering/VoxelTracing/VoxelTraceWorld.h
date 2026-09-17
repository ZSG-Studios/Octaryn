#pragma once
#include "VoxelTraceTypes.h"
#include "../../WorldPresentation/WorldStream/WorldStream.h"
#include <deque>
#include <map>
#include <memory>
#include <vector>

namespace octaryn::client::voxel_tracing {
struct TraceChunk {
  ChunkKey key;
  std::uint64_t geometry_epoch{};
  unsigned min_local_y{},max_local_y{};
  std::uint32_t macro_mask{};
  std::array<std::uint64_t,LeafCount> leaves{};
  std::array<std::uint64_t,MacroCount> macros{};
  std::shared_ptr<const world_presentation::StreamColumn> source;
  std::shared_ptr<const std::array<std::uint32_t,MaterialWordCount>> packed_materials;
  VoxelSample sample(unsigned x,unsigned y,unsigned z) const;
  std::span<const std::uint32_t> material_words() const;
  void export_payload(ChunkPayload&) const;
};
enum class ChangeKind {Published,Removed};
struct TraceChange {ChunkKey key;std::uint64_t geometry_epoch{};ChangeKind kind{};};
struct TraceLimits {std::size_t max_chunks{8192},max_changes{4096};unsigned max_column_height{1024};};
enum class PublishStatus {Published,Unchanged,Invalid,Capacity,Stale};
struct PublishResult {PublishStatus status{PublishStatus::Invalid};unsigned changed_chunks{};};
struct TraceColumnInput {
  std::shared_ptr<const world_presentation::StreamColumn> source,expected_source;
  std::vector<std::shared_ptr<const TraceChunk>> retained;
  unsigned max_column_height{};
};
struct PreparedTraceColumn {
  std::shared_ptr<const world_presentation::StreamColumn> source,expected_source;
  std::vector<std::shared_ptr<TraceChunk>> chunks;
  std::vector<bool> changed;
  PublishStatus status{PublishStatus::Invalid};
};
// Host-worker function: reads only immutable input snapshots, never owner maps.
PreparedTraceColumn prepare_trace_column(TraceColumnInput);

// Serialized presentation owner. Published chunk snapshots are immutable and may
// be retained by bounded upload jobs; no mesh, AS, device or private worker owner.
class VoxelTraceWorld {
public:
  explicit VoxelTraceWorld(TraceLimits limits={});
  PublishResult publish(const world_presentation::StreamColumn&);
  TraceColumnInput prepare_input(const world_presentation::StreamColumn&) const;
  PublishResult publish_prepared(PreparedTraceColumn);
  unsigned remove(std::int32_t column_x,std::int32_t column_z);
  VoxelSample sample(std::int64_t x,std::int64_t y,std::int64_t z) const;
  std::shared_ptr<const TraceChunk> find(ChunkKey) const;
  const auto& chunks() const {return chunks_;}
  std::uint64_t epoch() const {return epoch_;}
  // False means journal overflow: consumer must resynchronize from chunks().
  bool changes_since(std::uint64_t cursor,std::vector<TraceChange>&) const;
  std::size_t hierarchy_bytes() const;
private:
  using ColumnKey=std::pair<std::int32_t,std::int32_t>;
  TraceLimits limits_;
  std::uint64_t epoch_{};
  std::map<ColumnKey,std::shared_ptr<const world_presentation::StreamColumn>> columns_;
  std::map<ChunkKey,std::shared_ptr<const TraceChunk>> chunks_;
  std::deque<TraceChange> changes_;
  void changed(ChunkKey,ChangeKind);
};
std::shared_ptr<TraceChunk> build_trace_chunk(
  std::shared_ptr<const world_presentation::StreamColumn>,std::int32_t chunk_y);
bool equal_trace_content(const TraceChunk&,const TraceChunk&);
}
