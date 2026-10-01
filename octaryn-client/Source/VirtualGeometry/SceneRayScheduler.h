#pragma once
#include "SceneMemoryLedger.h"
#include <slang-rhi.h>
#include <memory>
#include <string>

namespace octaryn::client::rendering::virtual_geometry {
class SceneRayScheduler {
public:
  explicit SceneRayScheduler(std::shared_ptr<SceneMemoryLedger>);
  ~SceneRayScheduler();
  bool initialize(rhi::IDevice*,const char* shader_path);
  // FIFO ownership spans every build/compaction submission of one complete cut.
  bool acquire(const void* owner);
  bool release(const void* owner);
  bool submitted(const void* owner,rhi::IFence*,std::uint64_t);
  std::shared_ptr<SceneMemoryLease> reserve(const void* owner,std::uint64_t candidate_bytes,
      std::uint64_t scratch_bytes);
  rhi::IBuffer* scratch() const;
  rhi::IComputePipeline* expansion() const;
  std::shared_ptr<SceneMemoryLedger> ledger() const;
  const std::string& error() const;
private:
  struct State;std::unique_ptr<State> state_;
};
}
