#pragma once
#include "VoxelTraceUploadPlan.h"
#include <slang-rhi.h>

namespace octaryn::client::voxel_tracing {
struct UploadStats {
  std::uint64_t allocated_bytes{},reserved_bytes{},uploaded_bytes{},world_epoch{},generation{};
  unsigned capacity{},resident{},pending{},capacity_unknown{},uploaded_chunks{},evicted{},invalidated{},max_hash_probes{};
  bool journal_overflow{};
};
class VoxelTraceUpload {
public:
  bool initialize(rhi::IDevice*,UploadConfig config={});
  // Caller must have completed the selected frame-slot fence before prepare.
  bool prepare(rhi::ICommandEncoder*,const VoxelTraceWorld&,unsigned slot,ChunkKey center);
  bool bind(rhi::IShaderObject*,unsigned slot) const;
  const UploadStats& stats() const {return stats_;}
  const UploadLayout& layout() const {return layout_;}
private:
  struct Frame {
    Slang::ComPtr<rhi::IBuffer> chunks,leaves,macros,materials,hash;
    UploadSnapshot snapshot;
    bool allocated{};
  };
  Slang::ComPtr<rhi::IDevice> device_;
  std::array<Frame,2> frames_;
  UploadConfig config_;
  UploadLayout layout_;
  UploadStats stats_;
  bool allocate(Frame&);
};
}
