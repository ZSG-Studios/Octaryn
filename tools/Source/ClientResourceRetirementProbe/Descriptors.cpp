#include "Probe.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#ifdef _WIN32
#include "d3d12/d3d12-bindless-descriptor-set.h"
#endif

namespace retirement_probe {
void descriptor_cases() {
#ifdef _WIN32
  rhi::d3d12::BindlessDescriptorSet::SlotAllocator slots;
  constexpr unsigned Capacity=64,Threads=4,Iterations=1024,Batch=8;
  slots.capacity=Capacity;
  std::array<std::atomic<bool>,Capacity> live{};
  std::atomic<unsigned> failures{},completed{};
  std::vector<std::thread> threads;
  for(unsigned t=0;t<Threads;++t)threads.emplace_back([&] {
    for(unsigned iteration=0;iteration<Iterations;++iteration) {
      std::array<std::uint32_t,Batch> allocated{};
      unsigned count=0;
      for(;count<Batch;++count) {
        auto& slot=allocated[count];
        if(SLANG_FAILED(slots.allocate(&slot)) || slot>=Capacity) {++failures;break;}
        if(live[slot].exchange(true))++failures;
      }
      std::this_thread::yield();
      while(count) {
        const auto slot=allocated[--count];
        if(!live[slot].exchange(false) || SLANG_FAILED(slots.free(slot)))++failures;
        ++completed;
      }
    }
  });
  for(auto& thread:threads)thread.join();
  require(failures==0 && completed==Threads*Iterations*Batch,"concurrent bindless allocation/free corrupted slots");
  std::array<bool,Capacity> seen{};
  std::array<std::uint32_t,Capacity> allocated{};
  for(auto& slot:allocated) {
    require(SLANG_SUCCEEDED(slots.allocate(&slot)) && slot<Capacity && !seen[slot],
        "post-churn bindless capacity contains missing/duplicate slots");seen[slot]=true;
  }
  std::uint32_t overflow{};
  require(SLANG_FAILED(slots.allocate(&overflow)),"bindless allocator exceeded hard capacity");
  for(const auto slot:allocated)require(SLANG_SUCCEEDED(slots.free(slot)),"post-churn bindless release failed");
  require(SLANG_FAILED(slots.free(Capacity)),"out-of-range bindless slot accepted");
  std::printf("resource_retirement_descriptors=passed threads=%u operations=%u capacity=%u gpu_runtime=0\n",
      Threads,completed.load(),Capacity);
#else
  std::puts("resource_retirement_descriptors=unavailable backend=d3d12 platform=non_windows");
#endif
}
}
