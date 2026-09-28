#include "DeviceMemory.h"
#include "TileBudget.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>

using namespace octaryn::client::rendering;
int main() {
  rhi::DeviceInfo info{};
  const auto initial=device_memory_stats(info,true);
  assert(initial.reserved_capacity_bytes==0);
  auto owner=std::make_shared<DeviceMemoryReservation>(2433120);
  auto frame=owner;auto snapshot=owner;
  const auto held=device_memory_stats(info,false);
  assert(held.reserved_capacity_bytes==2433120);
  owner.reset();snapshot.reset();
  const auto refreshed=device_memory_stats(info,true);
  assert(refreshed.reserved_capacity_bytes==2433120);
  // The frame retains the charge through both pool release and an OS refresh.
  frame.reset();assert(device_memory_stats(info,false).reserved_capacity_bytes==0);
  auto maximum=std::make_shared<DeviceMemoryReservation>(std::numeric_limits<std::uint64_t>::max());
  bool rejected=false;
  try {auto overflow=std::make_shared<DeviceMemoryReservation>(1);}catch(const std::bad_alloc&) {rejected=true;}
  assert(rejected && device_memory_stats(info,false).reserved_capacity_bytes==std::numeric_limits<std::uint64_t>::max());
  maximum.reset();assert(device_memory_stats(info,false).reserved_capacity_bytes==0);
  assert(tile_gpu_admits(10,100,40,5,15));
  assert(!tile_gpu_admits(11,100,40,5,15));
  assert(!tile_gpu_admits(1,100,40,5,std::numeric_limits<std::uint64_t>::max()));
  assert(!tile_gpu_admits(1,std::numeric_limits<std::uint64_t>::max(),0,
      std::numeric_limits<std::uint64_t>::max(),std::numeric_limits<std::uint64_t>::max()));
  std::printf("gpu_reservation_tests passed=1 full_owner_lifetime=1 refresh_retains_credit=1 cached_sample_observed=%u overflow_rejected=1\n",
      initial.sample_id==held.sample_id?1u:0u);
}
