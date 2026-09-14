#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cmath>

namespace octaryn::client::rendering {
// Each timestamp pair follows the same fence slot as its recorded render work.
struct TemporalTiming {
  struct Slot {Slang::ComPtr<rhi::IQueryPool> queries;bool pending{};};
  std::array<Slot,2> slots;
  double milliseconds_per_tick{};
  bool initialize(rhi::IDevice* device) {
    const auto frequency=device->getInfo().timestampFrequency;
    if(!frequency || !device->hasFeature(rhi::Feature::TimestampQuery))return false;
    milliseconds_per_tick=1000.0/double(frequency);
    rhi::QueryPoolDesc desc{};desc.count=2;desc.label="temporal_resolution_timing";
    for(auto& slot:slots)if(!slot.queries &&
        SLANG_FAILED(device->createQueryPool(desc,slot.queries.writeRef())))return false;
    return true;
  }
  bool resolve(unsigned index,float& gpu_ms) {
    gpu_ms=0;auto& slot=slots.at(index);if(!slot.pending)return true;
    std::uint64_t ticks[2]{};
    if(SLANG_FAILED(slot.queries->getResult(0,2,ticks)))return false;
    slot.pending=false;
    if(ticks[1]>ticks[0])gpu_ms=float(double(ticks[1]-ticks[0])*milliseconds_per_tick);
    return true;
  }
  bool begin(rhi::ICommandEncoder* commands,unsigned index) {
    auto& slot=slots.at(index);
    if(!slot.queries || slot.pending || SLANG_FAILED(slot.queries->reset()))return false;
    commands->writeTimestamp(slot.queries,0);return true;
  }
  void end(rhi::ICommandEncoder* commands,unsigned index) {commands->writeTimestamp(slots.at(index).queries,1);}
  void submit(unsigned index) {slots.at(index).pending=true;}
  void reset() {for(auto& slot:slots)slot.pending=false;}
};
}
