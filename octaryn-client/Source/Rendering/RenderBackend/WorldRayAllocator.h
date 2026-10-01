#pragma once
#include "WorldRayBuildBudget.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
namespace octaryn::client::rendering {
struct WorldRayAllocation {
  Slang::ComPtr<rhi::IBuffer> bounds,scratch;
  Slang::ComPtr<rhi::IAccelerationStructure> blas;
  std::uint32_t faces{},buffers_created{};
  std::atomic<bool> complete{},cancelled{};
  std::atomic<std::uint64_t> blas_bytes{},buffer_bytes{};
  SlangResult result{SLANG_E_NOT_AVAILABLE};
  const char* step{"queued"};
  double milliseconds{};
  std::mutex mutex;
  std::condition_variable changed;
  bool wait(std::uint64_t timeout_ns);
};
// Device-only creation; encoding, submission and final ticket release stay on the owner.
class WorldRayAllocator {
  struct State;
  std::unique_ptr<State> state_;
public:
  static constexpr std::size_t Capacity=world_ray::BuildJobCapacity;
  using CreateAcceleration=SlangResult(*)(void*,const rhi::AccelerationStructureDesc&,rhi::IAccelerationStructure**);
  explicit WorldRayAllocator(rhi::IDevice*,CreateAcceleration=nullptr,void* context=nullptr);
  ~WorldRayAllocator();
  std::shared_ptr<WorldRayAllocation> request(std::uint32_t faces,rhi::IBuffer* bounds,rhi::IBuffer* scratch);
  void collect();
  void stop();
  bool finished() const;
  std::size_t pending() const;
  std::uint64_t bytes() const;
  std::uint64_t created() const;
  double milliseconds() const;
  bool finish(std::uint64_t timeout_ns);
};
}
