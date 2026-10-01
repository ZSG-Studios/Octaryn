#include "DeviceMemory.h"
#include <chrono>
#include <cstring>
#include <mutex>
#include <atomic>
#include <limits>
#include <new>
#include <cstdio>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dxgi1_4.h>
#include <psapi.h>
#include <wrl/client.h>
#endif

namespace octaryn::client::rendering {
namespace {std::atomic<std::uint64_t> capacity_reservations{};}
DeviceMemoryReservation::DeviceMemoryReservation(std::uint64_t bytes):bytes_(bytes) {
  auto current=capacity_reservations.load();
  do {
    if(bytes>std::numeric_limits<std::uint64_t>::max()-current)throw std::bad_alloc();
  }while(!capacity_reservations.compare_exchange_weak(current,current+bytes));
  std::printf("gpu_capacity_reservation acquired=%llu total=%llu\n",
      static_cast<unsigned long long>(bytes),static_cast<unsigned long long>(current+bytes));
}
DeviceMemoryReservation::~DeviceMemoryReservation() {
  const auto before=capacity_reservations.fetch_sub(bytes_);
  std::printf("gpu_capacity_reservation released=%llu total=%llu\n",
      static_cast<unsigned long long>(bytes_),static_cast<unsigned long long>(before-bytes_));
}
DeviceMemoryStats device_memory_stats(const rhi::DeviceInfo& info,bool refresh) {
#if defined(_WIN32)
  using Clock=std::chrono::steady_clock;
  struct Observer {
    std::mutex mutex;
    rhi::AdapterLUID luid{};
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
    DeviceMemoryStats stats;
    Clock::time_point sampled{};
    bool initialized{};
    std::uint64_t sequence{};
  };
  static Observer observer;
  std::lock_guard lock(observer.mutex);
  const auto now=Clock::now();
  if(!refresh && observer.initialized && observer.luid==info.adapterLUID &&
      now-observer.sampled<std::chrono::seconds(1)) {
    auto result=observer.stats;result.reserved_capacity_bytes=capacity_reservations.load();return result;
  }
  if(!observer.initialized || observer.luid!=info.adapterLUID) {
    observer.adapter.Reset();observer.luid=info.adapterLUID;
    // Resolve the optional diagnostics API without adding it to other platforms.
    static const auto module=LoadLibraryW(L"dxgi.dll");
    using CreateFactory=HRESULT(WINAPI*)(UINT,REFIID,void**);
    const auto create=module?reinterpret_cast<CreateFactory>(GetProcAddress(module,"CreateDXGIFactory2")):nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    if(create && SUCCEEDED(create(0,IID_PPV_ARGS(&factory)))) {
      LUID luid{};std::memcpy(&luid,info.adapterLUID.luid,sizeof(luid));
      factory->EnumAdapterByLuid(luid,IID_PPV_ARGS(&observer.adapter));
    }
    observer.initialized=true;
  }
  observer.stats={};observer.sampled=now;
  observer.stats.sample_id=++observer.sequence;
  DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
  if(observer.adapter && SUCCEEDED(observer.adapter->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memory))) {
    observer.stats.local_usage=memory.CurrentUsage;
    observer.stats.local_budget=memory.Budget;
    observer.stats.budget_available=true;
  }
  PROCESS_MEMORY_COUNTERS process{};process.cb=sizeof(process);
  if(K32GetProcessMemoryInfo(GetCurrentProcess(),&process,sizeof(process))) {
    observer.stats.process_resident=process.WorkingSetSize;
    observer.stats.process_peak=process.PeakWorkingSetSize;
  }
  observer.stats.reserved_capacity_bytes=capacity_reservations.load();return observer.stats;
#else
  (void)info;(void)refresh;DeviceMemoryStats result;
  result.reserved_capacity_bytes=capacity_reservations.load();return result;
#endif
}
}
