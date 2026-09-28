#pragma once
#include <slang-rhi.h>
#include <cstdint>
#include <memory>

namespace octaryn::client::rendering {
struct DeviceMemoryStats {
  std::uint64_t local_usage{},local_budget{},process_resident{},process_peak{};
  std::uint64_t sample_id{};
  std::uint64_t reserved_capacity_bytes{};
  bool budget_available{};
};
// OS budget observation, throttled to one sample per second. Not a GPU fence.
DeviceMemoryStats device_memory_stats(const rhi::DeviceInfo&,bool refresh=false);
// Conservative logical charge until every associated GPU resource owner releases it.
class DeviceMemoryReservation {
  std::uint64_t bytes_{};
public:
  explicit DeviceMemoryReservation(std::uint64_t);
  ~DeviceMemoryReservation();
  DeviceMemoryReservation(const DeviceMemoryReservation&)=delete;
  DeviceMemoryReservation& operator=(const DeviceMemoryReservation&)=delete;
};
}
