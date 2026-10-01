#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <charconv>
#include <memory>
#include <optional>
#include <string_view>

namespace octaryn::client::rendering::world_ray {
inline constexpr unsigned PrewarmMapCapacity=512,NormalItemCapacity=1000,MaximumItemCapacity=10000;
inline constexpr unsigned SceneFrameCount=2,SceneSnapshotCount=SceneFrameCount+1;
inline constexpr std::uint64_t MaximumPrewarmBytes=256ull*1024*1024;
inline std::optional<unsigned> item_prewarm_capacity(const char* value) {
  if(!value)return NormalItemCapacity;
  unsigned count{};const std::string_view text(value);
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),count);
  if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() ||
      (count!=NormalItemCapacity && count!=MaximumItemCapacity))return {};
  return count;
}
struct CapacityPlan {
  unsigned instances{};
  std::uint64_t instance_bytes{},scratch_bytes{},map_record_bytes{},record_bytes{},tlas_bytes{},total_bytes{};
};
inline std::uint64_t capacity_bytes(std::uint64_t bytes,unsigned stride) {
  if(!stride || bytes>MaximumPrewarmBytes)return 0;
  const auto elements=(std::max<std::uint64_t>(bytes,stride)+stride-1)/stride;
  return std::bit_ceil(elements)*stride;
}
inline std::optional<CapacityPlan> capacity_plan(unsigned maps,unsigned items,unsigned assets,unsigned instance_stride,
    unsigned map_stride,unsigned record_stride,std::uint64_t tlas,std::uint64_t scratch) {
  if(!maps || maps>4096 || !items || items>MaximumItemCapacity || !assets || assets>256 || !tlas)return {};
  CapacityPlan plan;plan.instances=maps+items+1;
  plan.instance_bytes=capacity_bytes(std::uint64_t(plan.instances)*instance_stride,instance_stride);
  plan.scratch_bytes=capacity_bytes(scratch,4);
  plan.map_record_bytes=capacity_bytes(std::uint64_t(maps+assets)*map_stride,map_stride);
  plan.record_bytes=capacity_bytes(record_stride,record_stride);
  plan.tlas_bytes=capacity_bytes(tlas,1);
  if(!plan.instance_bytes || !plan.scratch_bytes || !plan.map_record_bytes || !plan.record_bytes || !plan.tlas_bytes)return {};
  plan.total_bytes=SceneFrameCount*(plan.instance_bytes+plan.scratch_bytes)+
      SceneSnapshotCount*(plan.map_record_bytes+plan.record_bytes+plan.tlas_bytes);
  if(plan.total_bytes>MaximumPrewarmBytes)return {};
  return plan;
}
template<class T,std::size_t N> std::shared_ptr<T> exclusive_snapshot(const std::array<std::shared_ptr<T>,N>& pool) {
  for(const auto& entry:pool)if(entry && entry.use_count()==1)return entry;
  return {};
}
}
