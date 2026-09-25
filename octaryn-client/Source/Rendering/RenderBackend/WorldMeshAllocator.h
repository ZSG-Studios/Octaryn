#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
namespace octaryn::client::rendering {
struct WorldMeshAllocation {
  std::array<rhi::BufferDesc,3> descriptions;
  std::array<Slang::ComPtr<rhi::IBuffer>,3> buffers;
  std::atomic<bool> complete{},cancelled{};
  std::atomic<std::uint64_t> bytes{};
  std::uint32_t created{},empty_fluid_reuses{};
  bool empty_fluids{};
  SlangResult result{SLANG_E_NOT_AVAILABLE};
  double milliseconds{};
  std::mutex mutex;
  std::condition_variable changed;
  bool wait(std::uint64_t timeout_ns);
};
// One device-only producer. Queue encoding and publication stay on the owner thread.
class WorldMeshAllocator {
  struct State;
  std::unique_ptr<State> state_;
public:
  static constexpr std::size_t Capacity=17; // Eight delivery, eight halo, one qualification.
  using CreateBuffer=SlangResult(*)(void*,const rhi::BufferDesc&,rhi::IBuffer**);
  explicit WorldMeshAllocator(rhi::IDevice*,CreateBuffer=nullptr,void* context=nullptr);
  ~WorldMeshAllocator();
  std::shared_ptr<WorldMeshAllocation> request(const std::array<rhi::BufferDesc,3>&,bool empty_fluids=false);
  void collect(); // Owner-thread final release, including cancelled results.
  void stop();
  bool finished() const;
  std::size_t pending() const;
  bool finish(std::uint64_t timeout_ns);
};
}
